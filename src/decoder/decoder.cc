#include "decoder.hh"
#include "uncompressed_chunk.hh"
#include "frame.hh"

using namespace std;

Decoder::Decoder( const uint16_t width, const uint16_t height, const Chunk & key_frame )
  : width_( width ), height_( height ),
    state_( KeyFrame( UncompressedChunk( key_frame, width_, height_ ),
		      width_, height_ ).header(),
	    width_, height_ ),
    references_( width_, height_ )
{}

bool Decoder::decode_frame( const Chunk & frame, Raster & raster )
{
  /* parse uncompressed data chunk */
  UncompressedChunk uncompressed_chunk( frame, width_, height_ );

  if ( uncompressed_chunk.key_frame() ) {
    KeyFrame myframe( uncompressed_chunk, width_, height_ );
    state_ = DecoderState( myframe.header(), width_, height_ );

    myframe.parse_macroblock_headers( state_.segmentation_map, state_.probability_tables );
    myframe.parse_tokens( state_.quantizer_filter_adjustments, state_.probability_tables );
    myframe.decode( state_.quantizer_filter_adjustments, raster );

    myframe.copy_to( raster, references_ );
  } else {
    /* interframe */
    InterFrame myframe( uncompressed_chunk, width_, height_ );

    state_.quantizer_filter_adjustments.update( myframe.header() );
    state_.segmentation_map.update( myframe.header() );

    ProbabilityTables frame_probability_tables( state_.probability_tables );
    frame_probability_tables.update( myframe.header() );

    myframe.parse_macroblock_headers( state_.segmentation_map, frame_probability_tables );
    myframe.parse_tokens( state_.quantizer_filter_adjustments, frame_probability_tables );
    myframe.decode( state_.quantizer_filter_adjustments, references_, raster );

    myframe.copy_to( raster, references_ );

    if ( myframe.header().refresh_entropy_probs ) {
      state_.probability_tables = frame_probability_tables;
    }
  }

  return uncompressed_chunk.show_frame();
}

References::References( const uint16_t width, const uint16_t height )
  : last( width, height ),
    golden( width, height ),
    alternative_reference( width, height )
{}

ProbabilityTables::ProbabilityTables( const KeyFrameHeader & header )
  : coeff_probs( k_default_coeff_probs ),
    y_mode_probs( k_default_y_mode_probs ),
    uv_mode_probs( k_default_uv_mode_probs ),
    motion_vector_probs( k_default_mv_probs )
{
  coeff_prob_update( header );
}

template <unsigned int size>
static void assign( SafeArray< Probability, size > & dest, const Array< Unsigned<8>, size > & src )
{
  for ( unsigned int i = 0; i < size; i++ ) {
    dest.at( i ) = src.at( i );
  }
}

void ProbabilityTables::update( const InterFrameHeader & header )
{
  coeff_prob_update( header );

  /* update intra-mode probabilities in inter macroblocks */
  if ( header.intra_16x16_prob.initialized() ) {
    assign( y_mode_probs, header.intra_16x16_prob.get() );
  }

  if ( header.intra_chroma_prob.initialized() ) {
    assign( uv_mode_probs, header.intra_chroma_prob.get() );
  }

  /* update motion vector component probabilities */
  for ( uint8_t i = 0; i < header.mv_prob_update.size(); i++ ) {
    for ( uint8_t j = 0; j < header.mv_prob_update.at( i ).size(); j++ ) {
      const auto & prob = header.mv_prob_update.at( i ).at( j );
      if ( prob.initialized() ) {
	motion_vector_probs.at( i ).at( j ) = prob.get();
      }
    }
  }
}

template <class HeaderType>
void ProbabilityTables::coeff_prob_update( const HeaderType & header )
{
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
}

SegmentationMap::SegmentationMap( const KeyFrameHeader & header,
				  const unsigned int macroblock_width,
				  const unsigned int macroblock_height )
  : mb_segment_tree_probs( {{ 255, 255, 255 }} ),
    map( macroblock_width, macroblock_height )
{
  update( header );
}

template <class HeaderType>
void SegmentationMap::update( const HeaderType & header )
{
  /* segmentation tree probabilities (if given in frame header) */
  if ( header.update_segmentation.initialized()
       and header.update_segmentation.get().mb_segmentation_map.initialized() ) {
    const auto & seg_map = header.update_segmentation.get().mb_segmentation_map.get();
    for ( unsigned int i = 0; i < mb_segment_tree_probs.size(); i++ ) {
      mb_segment_tree_probs.at( i ) = seg_map.at( i ).get_or( 255 );
    }
  }

  /* leave segmentation_map alone -- this needs to be updated by the macroblock */
}

QuantizerFilterAdjustments::QuantizerFilterAdjustments( const KeyFrameHeader & header )
  : segment_quantizer_adjustments( {{ }} ),
    segment_filter_adjustments( {{ }} ),
    loopfilter_ref_adjustments( {{ }} ),
    loopfilter_mode_adjustments( {{ }} )
{
  update( header );
}

template <class HeaderType>
void QuantizerFilterAdjustments::update( const HeaderType & header )
{
  /* update segment adjustments */
  for ( uint8_t i = 0; i < num_segments; i++ ) {
    segment_quantizer_adjustments.at( i ).update( i, header.update_segmentation );
    segment_filter_adjustments.at( i ).update( i, header.update_segmentation );
  }

  /* update additional in-loop deblocking filter adjustments */
  if ( header.mode_lf_adjustments.initialized()
       and header.mode_lf_adjustments.get().initialized() ) {
    for ( unsigned int i = 0; i < loopfilter_ref_adjustments.size(); i++ ) {
      loopfilter_ref_adjustments.at( i ) = header.mode_lf_adjustments.get().get().ref_update.at( i ).get_or( 0 );
      loopfilter_mode_adjustments.at( i ) = header.mode_lf_adjustments.get().get().mode_update.at( i ).get_or( 0 );
    }
  }
}

DecoderState::DecoderState( const KeyFrameHeader & header,
			    const unsigned int width,
			    const unsigned int height )
  : quantizer_filter_adjustments( header ),
    probability_tables( header ),
    segmentation_map( header,
		      Raster::macroblock_dimension( width ),
		      Raster::macroblock_dimension( height ) )
{}
