#include "decoder.hh"
#include "uncompressed_chunk.hh"
#include "frame.hh"

using namespace std;

Decoder::Decoder( uint16_t s_width, uint16_t s_height, const Chunk & key_frame )
  : width_( s_width ), height_( s_height ),
    state_( KeyFrame( UncompressedChunk( key_frame, width_, height_ ), width_, height_ ).header() )
{}

bool Decoder::decode_frame( const Chunk & frame, Raster & raster )
{
  /* parse uncompressed data chunk */
  UncompressedChunk uncompressed_chunk( frame, width_, height_ );

  /* only parse key frames for now */
  if ( !uncompressed_chunk.key_frame() ) {
    InterFrame myframe( uncompressed_chunk, width_, height_ );
    state_.update( myframe.header() );
    myframe.parse_macroblock_headers( state_ );
    myframe.parse_tokens( state_ );
    myframe.decode( state_, raster );

    return uncompressed_chunk.show_frame();
  }

  KeyFrame myframe( uncompressed_chunk, width_, height_ );
  state_ = DecoderState( myframe.header() );

  myframe.parse_macroblock_headers( state_ );
  myframe.parse_tokens( state_ );

  myframe.decode( state_, raster );

  return uncompressed_chunk.show_frame();
}

DecoderState::DecoderState( const KeyFrameHeader & header )
  : mb_segment_tree_probs( {{ 255, 255, 255 }} ),
    coeff_probs( k_default_coeff_probs ),
    segment_quantizer_adjustments( {{ QuantizerAdjustment( 0, header.update_segmentation ),
	    QuantizerAdjustment( 1, header.update_segmentation ),
	    QuantizerAdjustment( 2, header.update_segmentation ),
	    QuantizerAdjustment( 3, header.update_segmentation ) }} ),
    segment_filter_adjustments( {{ SegmentFilterAdjustment( 0, header.update_segmentation ),
	    SegmentFilterAdjustment( 1, header.update_segmentation ),
	    SegmentFilterAdjustment( 2, header.update_segmentation ),
	    SegmentFilterAdjustment( 3, header.update_segmentation ) }} ),
  loopfilter_ref_adjustments( {{ }} ),
  loopfilter_mode_adjustments( {{ }} ),
  y_mode_probs( k_default_y_mode_probs ),
  b_mode_probs( default_b_mode_probs ),
  uv_mode_probs( k_default_uv_mode_probs )
{
  common_update( header );
}

template <class HeaderType>
void DecoderState::common_update( const HeaderType & header )
{
  /* segmentation tree probabilities (if given in frame header) */
  if ( header.update_segmentation.initialized()
       and header.update_segmentation.get().mb_segmentation_map.initialized() ) {
    const auto & seg_map = header.update_segmentation.get().mb_segmentation_map.get();
    for ( unsigned int i = 0; i < mb_segment_tree_probs.size(); i++ ) {
      if ( seg_map.at( i ).initialized() ) {
	mb_segment_tree_probs.at( i ) = seg_map.at( i ).get();
      }
    }
  }

  /* token probabilities (if given in frame header) */
  for ( unsigned int i = 0; i < BLOCK_TYPES; i++ ) {
    for ( unsigned int j = 0; j < COEF_BANDS; j++ ) {
      for ( unsigned int k = 0; k < PREV_COEF_CONTEXTS; k++ ) {
	for ( unsigned int l = 0; l < ENTROPY_NODES; l++ ) {
	  const auto & node = header.token_prob_update.at( i ).at( j ).at( k ).at( l ).coeff_prob;
	  if ( node.initialized() ) {
	    coeff_probs.at( i ).at( j ).at( k ).at( l ) = node.get();
	  }
	}
      }
    }
  }

  if ( header.mode_lf_adjustments.initialized()
       and header.mode_lf_adjustments.get().initialized() ) {
    for ( unsigned int i = 0; i < loopfilter_ref_adjustments.size(); i++ ) {
      loopfilter_ref_adjustments.at( i ) = header.mode_lf_adjustments.get().get().ref_update.at( i ).get_or( 0 );
      loopfilter_mode_adjustments.at( i ) = header.mode_lf_adjustments.get().get().mode_update.at( i ).get_or( 0 );
    }
  }
}

template <unsigned int size>
static void assign( SafeArray< Probability, size > & dest, const Array< Unsigned<8>, size > & src )
{
  for ( unsigned int i = 0; i < size; i++ ) {
    dest.at( i ) = src.at( i );
  }
}

void DecoderState::update( const InterFrameHeader & header )
{
  /* update segment adjustments */
  for ( uint8_t i = 0; i < num_segments; i++ ) {
    segment_quantizer_adjustments.at( i ).update( i, header.update_segmentation );
    segment_filter_adjustments.at( i ).update( i, header.update_segmentation );
  }

  /* update intra-mode probabilities in inter macroblocks */
  if ( header.intra_16x16_prob.initialized() ) {
    assign( y_mode_probs, header.intra_16x16_prob.get() );
  }

  if ( header.intra_chroma_prob.initialized() ) {
    assign( uv_mode_probs, header.intra_chroma_prob.get() );
  }

  common_update( header );
}
