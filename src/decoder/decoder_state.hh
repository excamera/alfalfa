#ifndef DECODER_STATE_HH
#define DECODER_STATE_HH

#include "decoder.hh"
#include "frame.hh"

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

template <class HeaderType>
void QuantizerFilterAdjustments::update( const HeaderType & header )
{
  /* update segment adjustments */
  if ( header.update_segmentation.initialized() ) {
    if ( header.update_segmentation.get().segment_feature_data.initialized() ) {
      const auto & feature_data = header.update_segmentation.get().segment_feature_data.get();

      absolute_segment_adjustments = feature_data.segment_feature_mode;

      for ( uint8_t i = 0; i < num_segments; i++ ) {
	segment_quantizer_adjustments.at( i ) = feature_data.quantizer_update.at( i ).get_or( 0 );
	segment_filter_adjustments.at( i ) = feature_data.loop_filter_update.at( i ).get_or( 0 );
      }
    }
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

template <>
inline KeyFrame DecoderState::parse_and_apply<KeyFrame>( const UncompressedChunk & uncompressed_chunk )
{
  assert( uncompressed_chunk.key_frame() );

  /* parse keyframe header */
  KeyFrame myframe( uncompressed_chunk, width, height );

  /* reset persistent decoder state to default values */
  *this = DecoderState( myframe.header(), width, height );

  /* calculate new probability tables. replace persistent copy if prescribed in header */
  ProbabilityTables frame_probability_tables( probability_tables );
  frame_probability_tables.coeff_prob_update( myframe.header() );
  if ( myframe.header().refresh_entropy_probs ) {
    probability_tables = frame_probability_tables;
  }

  /* parse the frame (and update the persistent segmentation map) */
  myframe.parse_macroblock_headers_and_update_segmentation_map( segmentation_map, frame_probability_tables );
  myframe.parse_tokens( frame_probability_tables );

  return myframe;
}

template <>
inline InterFrame DecoderState::parse_and_apply<InterFrame>( const UncompressedChunk & uncompressed_chunk )
{
  assert( not uncompressed_chunk.key_frame() );

  /* parse interframe header */
  InterFrame myframe( uncompressed_chunk, width, height );

  /* update adjustments to quantizer and in-loop deblocking filter */
  quantizer_filter_adjustments.update( myframe.header() );

  /* update probability tables. replace persistent copy if prescribed in header */
  ProbabilityTables frame_probability_tables( probability_tables );
  frame_probability_tables.update( myframe.header() );
  if ( myframe.header().refresh_entropy_probs ) {
    probability_tables = frame_probability_tables;
  }

  /* parse the frame (and update the persistent segmentation map) */
  myframe.parse_macroblock_headers_and_update_segmentation_map( segmentation_map, frame_probability_tables );
  myframe.parse_tokens( frame_probability_tables );

  return myframe;
}

#endif /* DECODER_STATE_HH */
