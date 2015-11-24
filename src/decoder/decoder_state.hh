#ifndef DECODER_STATE_HH
#define DECODER_STATE_HH

#include "decoder.hh"
#include "frame.hh"

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
		      width, height, first_partition );

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

template <>
inline StateUpdateFrame DecoderState::parse_and_apply<StateUpdateFrame>( const UncompressedChunk & uncompressed_chunk )
{
  assert( not uncompressed_chunk.key_frame() );

  /* initialize Boolean decoder for the frame and macroblock headers */
  BoolDecoder first_partition( uncompressed_chunk.first_partition() );

  /* parse interframe header */
  StateUpdateFrame myframe( false, width, height, first_partition );

  probability_tables.update( myframe.header() );

  return myframe;
}

template <>
inline RefUpdateFrame DecoderState::parse_and_apply<RefUpdateFrame>( const UncompressedChunk & uncompressed_chunk )
{
  assert( not uncompressed_chunk.key_frame() );

  BoolDecoder first_partition( uncompressed_chunk.first_partition() );

  RefUpdateFrame myframe( false, width, height, first_partition );

  ProbabilityTables frame_probability_tables( probability_tables );
  frame_probability_tables.coeff_prob_update( myframe.header() );

  /* parse the frame */
  myframe.parse_macroblock_headers( first_partition, frame_probability_tables );

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
