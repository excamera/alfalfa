#include "encoder.hh"
#include "uncompressed_chunk.hh"
#include "frame.hh"

using namespace std;

Encoder::Encoder( const uint16_t width, const uint16_t height, const Chunk & key_frame )
  : width_( width ), height_( height ),
    state_( KeyFrame( UncompressedChunk( key_frame, width_, height_ ),
		      width_, height_ ).header(),
	    width_, height_ )
{}

vector< uint8_t > Encoder::encode_frame( const Chunk & frame )
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
    myframe.parse_tokens( state_.quantizer_filter_adjustments, frame_probability_tables );
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
    myframe.parse_tokens( state_.quantizer_filter_adjustments, frame_probability_tables );
  }

  return vector< uint8_t >();
}
