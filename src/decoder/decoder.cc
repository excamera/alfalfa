#include "decoder.hh"
#include "uncompressed_chunk.hh"
#include "frame.hh"

#include "decoder_state.cc"

using namespace std;

Decoder::Decoder( const uint16_t width, const uint16_t height, const Chunk & key_frame )
  : width_( width ), height_( height ),
    state_( KeyFrame( UncompressedChunk( key_frame, width_, height_ ),
		      width_, height_ ).header(),
	    width_, height_ ),
    references_( width_, height_ )
{}

bool Decoder::decode_frame( const Chunk & frame, RasterHandle & raster )
{
  /* parse uncompressed data chunk */
  UncompressedChunk uncompressed_chunk( frame, width_, height_ );

  if ( uncompressed_chunk.key_frame() ) {
    /* parse keyframe header */
    KeyFrame myframe( uncompressed_chunk, width_, height_ );

    /* reset persistent decoder state to default values */
    state_ = DecoderState( myframe.header(), width_, height_ );

    /* calculate new probability tables. replace persistent copy if prescribed in header */
    ProbabilityTables frame_probability_tables( state_.probability_tables );
    frame_probability_tables.coeff_prob_update( myframe.header() );
    if ( myframe.header().refresh_entropy_probs ) {
      state_.probability_tables = frame_probability_tables;
    }

    /* decode the frame (and update the persistent segmentation map) */
    myframe.parse_macroblock_headers_and_update_segmentation_map( state_.segmentation_map, frame_probability_tables );
    myframe.parse_tokens( frame_probability_tables );
    myframe.decode( state_.quantizer_filter_adjustments, raster );

    /* replace all the reference frames */
    myframe.copy_to( raster, references_ );
  } else {
    /* parse interframe header */
    InterFrame myframe( uncompressed_chunk, width_, height_ );

    /* update adjustments to quantizer and in-loop deblocking filter */
    state_.quantizer_filter_adjustments.update( myframe.header() );

    /* update probability tables. replace persistent copy if prescribed in header */
    ProbabilityTables frame_probability_tables( state_.probability_tables );
    frame_probability_tables.update( myframe.header() );
    if ( myframe.header().refresh_entropy_probs ) {
      state_.probability_tables = frame_probability_tables;
    }

    /* decode the frame (and update the persistent segmentation map) */
    myframe.parse_macroblock_headers_and_update_segmentation_map( state_.segmentation_map, frame_probability_tables );
    myframe.parse_tokens( frame_probability_tables );
    myframe.decode( state_.quantizer_filter_adjustments, references_, raster );

    /* update the reference frames as appropriate */
    myframe.copy_to( raster, references_ );
  }

  return uncompressed_chunk.show_frame();
}

References::References( const uint16_t width, const uint16_t height )
  : last( width, height ),
    golden( width, height ),
    alternative_reference( width, height )
{}

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

DecoderState::DecoderState( const KeyFrameHeader & header,
			    const unsigned int width,
			    const unsigned int height )
  : quantizer_filter_adjustments( header ),
    segmentation_map( Raster::macroblock_dimension( width ),
		      Raster::macroblock_dimension( height ) )
{}
