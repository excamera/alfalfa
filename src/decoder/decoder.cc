#include "decoder.hh"
#include "uncompressed_chunk.hh"
#include "frame.hh"
#include "decoder_state.hh"

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
    const FrameHandle< KeyFrame > myframe = state_.parse_and_apply<KeyFrame>( uncompressed_chunk );

    myframe->decode( state_.segmentation, state_.filter_adjustments, raster );

    myframe->copy_to( raster, references_ );

    return myframe->show_frame();
  } else {
    const FrameHandle< InterFrame > myframe = state_.parse_and_apply<InterFrame>( uncompressed_chunk );

    myframe->decode( state_.segmentation, state_.filter_adjustments, references_, raster );

    myframe->copy_to( raster, references_ );

    return myframe->show_frame();
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
  map.copy( other.map );
}
