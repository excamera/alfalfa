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

template <unsigned int size>
static void assign( SafeArray< Probability, size > & dest, const Array< Unsigned<8>, size > & src )
{
  for ( unsigned int i = 0; i < size; i++ ) {
    dest.at( i ) = src.at( i );
  }
}

template <class HeaderType>
void ProbabilityTables::update( const HeaderType & header )
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
void Segmentation::update( const HeaderType & header )
{
  assert( header.update_segmentation.initialized() );

  /* update segment adjustments */
  if ( header.update_segmentation.get().segment_feature_data.initialized() ) {
    const auto & feature_data = header.update_segmentation.get().segment_feature_data.get();

    absolute_segment_adjustments = feature_data.segment_feature_mode;

    for ( uint8_t i = 0; i < num_segments; i++ ) {
      segment_quantizer_adjustments.at( i ) = feature_data.quantizer_update.at( i ).get_or( 0 );
      segment_filter_adjustments.at( i ) = feature_data.loop_filter_update.at( i ).get_or( 0 );
    }
  }
}

template <class HeaderType>
void FilterAdjustments::update( const HeaderType & header ) {
  assert( header.mode_lf_adjustments.initialized() );

  /* update additional in-loop deblocking filter adjustments */
  if ( header.mode_lf_adjustments.get().initialized() ) {
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

  /* initialize Boolean decoder for the frame and macroblock headers */
  BoolDecoder first_partition( uncompressed_chunk.first_partition() );

  if ( uncompressed_chunk.experimental() ) {
    throw Invalid( "experimental key frame" );
  }

  /* parse keyframe header */
  KeyFrame myframe( uncompressed_chunk.show_frame(),
		    uncompressed_chunk.experimental(),
		    width, height, first_partition );

  /* reset persistent decoder state to default values */
  *this = DecoderState( myframe.header(), width, height );

  /* calculate new probability tables. replace persistent copy if prescribed in header */
  ProbabilityTables frame_probability_tables( probability_tables );
  frame_probability_tables.coeff_prob_update( myframe.header() );
  if ( myframe.header().refresh_entropy_probs ) {
    probability_tables = frame_probability_tables;
  }

  /* parse the frame (and update the persistent segmentation map) */
  myframe.parse_macroblock_headers( first_partition, frame_probability_tables );

  if ( segmentation.initialized() ) {
    myframe.update_segmentation( segmentation.get().map );
  }

  myframe.parse_tokens( uncompressed_chunk.dct_partitions( myframe.dct_partition_count() ),
			frame_probability_tables );

  return myframe;
}

template <>
inline InterFrame DecoderState::parse_and_apply<InterFrame>( const UncompressedChunk & uncompressed_chunk )
{
  assert( not uncompressed_chunk.key_frame() );

  /* initialize Boolean decoder for the frame and macroblock headers */
  BoolDecoder first_partition( uncompressed_chunk.first_partition() );

  /* parse interframe header */
  InterFrame myframe( uncompressed_chunk.show_frame(),
		      uncompressed_chunk.experimental(),
		      width, height, first_partition );

  /* update probability tables if prescribed by continuation header */
  if ( myframe.continuation_header().initialized()
       // FIXME ask keith replacement_entropy_header should not be optional?
       and myframe.continuation_header().get().replacement_entropy_header.initialized() ) {
    probability_tables.update( myframe.continuation_header().get().replacement_entropy_header.get() );
  }

  /* update probability tables. replace persistent copy if prescribed in header */
  ProbabilityTables frame_probability_tables( probability_tables );
  frame_probability_tables.update( myframe.header() );
  if ( myframe.header().refresh_entropy_probs ) {
    probability_tables = frame_probability_tables;
  }

  /* update adjustments to in-loop deblocking filter */
  if ( myframe.header().mode_lf_adjustments.initialized() ) {
    if ( filter_adjustments.initialized() ) {
      filter_adjustments.get().update( myframe.header() );
    } else {
      filter_adjustments.initialize( myframe.header() );
    }
  } else {
    filter_adjustments.clear();
  }

  /* update segmentation information */
  if ( myframe.header().update_segmentation.initialized() ) {
    if ( segmentation.initialized() ) {
      segmentation.get().update( myframe.header() );
    } else {
      segmentation.initialize( myframe.header(), width, height );
    }
  } else {
    segmentation.clear();
  }

  /* parse the frame (and update the persistent segmentation map) */
  myframe.parse_macroblock_headers( first_partition, frame_probability_tables );

  if ( segmentation.initialized() ) {
    myframe.update_segmentation( segmentation.get().map );
  }

  myframe.parse_tokens( uncompressed_chunk.dct_partitions( myframe.dct_partition_count() ),
			frame_probability_tables );

  return myframe;
}

template <class HeaderType>
Segmentation::Segmentation( const HeaderType & header,
			    const unsigned int width,
			    const unsigned int height )
  : map( width, height, 3 )
{
  update( header );
}

#endif /* DECODER_STATE_HH */
