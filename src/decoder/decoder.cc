#include "decoder.hh"
#include "uncompressed_chunk.hh"
#include "frame.hh"
#include "decoder_state.hh"
#include "diff_generator.hh"

using namespace std;

Decoder::Decoder( const uint16_t width, const uint16_t height )
  : state_( width, height ),
    references_( width, height )
{}

bool Decoder::decode_frame( const Chunk & frame, RasterHandle & raster )
{
  /* parse uncompressed data chunk */
  UncompressedChunk uncompressed_chunk( frame, state_.width, state_.height );

  if ( uncompressed_chunk.key_frame() ) {
    const KeyFrame myframe = state_.parse_and_apply<KeyFrame>( uncompressed_chunk );

    myframe.decode( state_.segmentation, raster );

    myframe.loopfilter( state_.segmentation, state_.filter_adjustments, raster );

    myframe.copy_to( raster, references_ );

    return myframe.show_frame();
  } else {
    const InterFrame myframe = state_.parse_and_apply<InterFrame>( uncompressed_chunk );

    myframe.decode( state_.segmentation, references_, raster );

    myframe.loopfilter( state_.segmentation, state_.filter_adjustments, raster );

    myframe.copy_to( raster, references_ );

    return myframe.show_frame();
  }
}

References::References( const uint16_t width, const uint16_t height )
  : last( width, height ),
    golden( width, height ),
    alternative_reference( width, height )
{}

DecoderState::DecoderState( const unsigned int s_width, const unsigned int s_height )
  : width( s_width ), height( s_height )
{}

DecoderState::DecoderState( const KeyFrameHeader & header,
			    const unsigned int s_width,
			    const unsigned int s_height )
  : width( s_width ), height( s_height ),
    segmentation( header.update_segmentation.initialized(), header, width, height ),
    filter_adjustments( header.mode_lf_adjustments.initialized(), header )
{}

bool DecoderState::operator==( const DecoderState & other ) const
{
  return width == other.width
    and height == other.height
    and probability_tables == other.probability_tables
    and segmentation == other.segmentation
    and filter_adjustments == other.filter_adjustments;
}

bool ProbabilityTables::operator==( const ProbabilityTables & other ) const
{
  return coeff_probs == other.coeff_probs
    and y_mode_probs == other.y_mode_probs
    and uv_mode_probs == other.uv_mode_probs
    and motion_vector_probs == other.motion_vector_probs;
}

bool FilterAdjustments::operator==( const FilterAdjustments & other ) const
{
  return loopfilter_ref_adjustments == other.loopfilter_ref_adjustments
    and loopfilter_mode_adjustments == other.loopfilter_mode_adjustments;
}

bool Segmentation::operator==( const Segmentation & other ) const
{
  return absolute_segment_adjustments == other.absolute_segment_adjustments
    and segment_quantizer_adjustments == other.segment_quantizer_adjustments
    and segment_filter_adjustments == other.segment_filter_adjustments
    and map == other.map;
}

Segmentation::Segmentation( const Segmentation & other )
  : absolute_segment_adjustments( other.absolute_segment_adjustments ),
    segment_quantizer_adjustments( other.segment_quantizer_adjustments ),
    segment_filter_adjustments( other.segment_filter_adjustments ),
    map( other.map.width(), other.map.height() )
{
  map.copy_from( other.map );
}

DiffGenerator::DiffGenerator( const uint16_t width, const uint16_t height )
  : Decoder( width, height ),
    on_key_frame_ { true }
{}

bool DiffGenerator::decode_frame( const Chunk & frame, RasterHandle & raster )
{
  /* parse uncompressed data chunk */
  UncompressedChunk uncompressed_chunk( frame, state_.width, state_.height );

  if ( uncompressed_chunk.key_frame() ) {
    on_key_frame_ = true;

    key_frame_ = state_.parse_and_apply<KeyFrame>( uncompressed_chunk );

    key_frame_.get().decode( state_.segmentation, raster );

    // Store copy of the raster for diffs before loopfilter is applied
    raster_.get().copy_from( raster );

    key_frame_.get().loopfilter( state_.segmentation, state_.filter_adjustments, raster );

    key_frame_.get().copy_to( raster, references_ );

    return key_frame_.get().show_frame();
  } else {
    on_key_frame_ = false;

    inter_frame_ = state_.parse_and_apply<InterFrame>( uncompressed_chunk );

    inter_frame_.get().decode( state_.segmentation, references_, raster );

    raster_.get().copy_from( raster );

    inter_frame_.get().loopfilter( state_.segmentation, state_.filter_adjustments, raster );

    inter_frame_.get().copy_to( raster, references_ );

    return inter_frame_.get().show_frame();
  }
}

RasterHandle DiffGenerator::decode_diff( const Chunk & frame ) const
{
  // Operate on copy of state so operation is const
  DecoderState diff_state = state_;

  References diff_refs( state_.width, state_.height );
  diff_refs.last = diff_refs.golden = diff_refs.alternative_reference = raster_;

  RasterHandle raster( state_.width, state_.height );

  UncompressedChunk uncompressed_chunk( frame, state_.width, state_.height );

  if ( uncompressed_chunk.key_frame() ) {
    const KeyFrame myframe = diff_state.parse_and_apply<KeyFrame>( uncompressed_chunk );

    myframe.decode( diff_state.segmentation, raster );

    myframe.loopfilter( diff_state.segmentation, diff_state.filter_adjustments, raster );
  } else {
    const InterFrame myframe = diff_state.parse_and_apply<InterFrame>( uncompressed_chunk );

    myframe.decode( diff_state.segmentation, diff_refs, raster );

    myframe.loopfilter( diff_state.segmentation, diff_state.filter_adjustments, raster );
  }

  return raster;
}

// This needs to be made const, which means rewriting rewrite_as_diff into make_continuation_frame
vector< uint8_t > DiffGenerator::operator-( const DiffGenerator & source_decoder )
{
  if ( on_key_frame_ ) {
    return key_frame_.get().serialize( state_.probability_tables );
  } else {
    inter_frame_.get().rewrite_as_diff( source_decoder.state_, state_, references_,
					source_decoder.raster_, raster_ );

    inter_frame_.get().optimize_continuation_coefficients();

    return inter_frame_.get().serialize( state_.probability_tables );
  }
}
