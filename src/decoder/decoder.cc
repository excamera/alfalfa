#include "decoder.hh"
#include "uncompressed_chunk.hh"
#include "frame.hh"
#include "decoder_state.hh"

#include <sstream>

using namespace std;

Decoder::Decoder( const uint16_t width, const uint16_t height )
  : state_( width, height ),
    references_( width, height )
{}

UncompressedChunk Decoder::decompress_frame( const Chunk & compressed_frame ) const
{
  /* parse uncompressed data chunk */
  return UncompressedChunk( compressed_frame, state_.width, state_.height );
}

template<class FrameType>
FrameType Decoder::parse_frame( const UncompressedChunk & decompressed_frame )
{
  return state_.parse_and_apply<FrameType>( decompressed_frame );
}
template KeyFrame Decoder::parse_frame<KeyFrame>( const UncompressedChunk & decompressed_frame );
template InterFrame Decoder::parse_frame<InterFrame>( const UncompressedChunk & decompressed_frame );
template RefUpdateFrame Decoder::parse_frame<RefUpdateFrame>( const UncompressedChunk & decompressed_frame );
template StateUpdateFrame Decoder::parse_frame<StateUpdateFrame>( const UncompressedChunk & decompressed_frame );

/* Some callers (such as the code that produces SerializedFrames) needs the output Raster
 * regardless of whether or not it is shown, so return a pair with a bool indicating if the
 * output should be shown followed by the actual output. parse_and_decode_frame takes care of
 * turning the pair into an Optional for the normal case */
template<class FrameType>
pair<bool, RasterHandle> Decoder::decode_frame( const FrameType & frame )
{
  /* get a RasterHandle */
  MutableRasterHandle raster { state_.width, state_.height };

  const bool shown = frame.show_frame();

  frame.decode( state_.segmentation, references_, raster );

  frame.loopfilter( state_.segmentation, state_.filter_adjustments, raster );

  RasterHandle immutable_raster( move( raster ) );

  frame.copy_to( immutable_raster, references_ );

  return make_pair( shown, immutable_raster );
}
template pair<bool, RasterHandle> Decoder::decode_frame<KeyFrame>( const KeyFrame & frame );
template pair<bool, RasterHandle> Decoder::decode_frame<InterFrame>( const InterFrame & frame );
template pair<bool, RasterHandle> Decoder::decode_frame<RefUpdateFrame>( const RefUpdateFrame & frame );
template pair<bool, RasterHandle> Decoder::decode_frame<StateUpdateFrame>( const StateUpdateFrame & frame );

/* This function takes care of the full decoding process from decompressing the Chunk
 * to returning an Optional<RasterHandle> as the output
 */
Optional<RasterHandle> Decoder::parse_and_decode_frame( const Chunk & compressed_frame )
{
  UncompressedChunk decompressed_frame = decompress_frame( compressed_frame );
  if ( decompressed_frame.key_frame() ) {
    auto output = decode_frame( parse_frame<KeyFrame>( decompressed_frame ) );
    return make_optional( output.first, output.second );
  } else if ( decompressed_frame.experimental() ) {
    if ( decompressed_frame.reference_update() ) {
      auto output = decode_frame( parse_frame<RefUpdateFrame>( decompressed_frame ) );
      return make_optional( output.first, output.second );
    } else {
      auto output = decode_frame( parse_frame<StateUpdateFrame>( decompressed_frame ) );
      return make_optional( output.first, output.second );
    }
  } else {
    auto output = decode_frame( parse_frame<InterFrame>( decompressed_frame ) );
    return make_optional( output.first, output.second );
  }
}

SourceHash Decoder::source_hash( const DependencyTracker & deps ) const
{
  using OptHash = Optional<size_t>;

  return SourceHash( deps.need_state ? OptHash( true, state_.hash() ) : OptHash( false ),
                     deps.need_last ? OptHash( true, references_.last.hash() ) : OptHash( false ),
                     deps.need_golden ? OptHash( true, references_.golden.hash() ) : OptHash( false ),
                     deps.need_alternate ? OptHash( true, references_.alternative_reference.hash() ) : OptHash( false ) );
}

TargetHash Decoder::target_hash( const UpdateTracker & updates, const RasterHandle & output,
                                 bool shown ) const
{
  return TargetHash( updates, state_.hash(), output.hash(), shown );
}

DecoderHash Decoder::get_hash( void ) const
{
  return DecoderHash( state_.hash(), references_.last.hash(),
                      references_.golden.hash(), references_.alternative_reference.hash() );
}

References Decoder::get_references( void ) const
{
  return references_;
}

bool Decoder::operator==( const Decoder & other ) const
{
  return state_ == other.state_ and references_.last == other.references_.last and
    references_.golden == other.references_.golden and
    references_.alternative_reference == other.references_.alternative_reference;
}
   
References::References( const uint16_t width, const uint16_t height )
  : References( MutableRasterHandle( width, height ) )
{}

References::References( MutableRasterHandle && raster )
  : last( move( raster ) ),
    golden( last ),
    alternative_reference( last )
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

size_t DecoderState::hash( void ) const
{
  size_t hash_val = 0;

  boost::hash_combine( hash_val, width );
  boost::hash_combine( hash_val, height );
  boost::hash_combine( hash_val, probability_tables.hash() );
  if ( segmentation.initialized() ) {
    boost::hash_combine( hash_val, segmentation.get().hash() );
  }
  if ( filter_adjustments.initialized() ) {
    boost::hash_combine( hash_val, filter_adjustments.get().hash() );
  }

  return hash_val;
}

size_t FilterAdjustments::hash( void ) const
{
  size_t hash_val = 0;

  boost::hash_range( hash_val, loopfilter_ref_adjustments.begin(), loopfilter_ref_adjustments.end() );

  boost::hash_range( hash_val, loopfilter_mode_adjustments.begin(), loopfilter_ref_adjustments.end() );

  return hash_val;
}

size_t Segmentation::hash( void ) const
{
  size_t hash_val = 0;

  boost::hash_combine( hash_val, absolute_segment_adjustments );

  boost::hash_range( hash_val, segment_quantizer_adjustments.begin(),
		       segment_quantizer_adjustments.end() );

  boost::hash_range( hash_val, segment_filter_adjustments.begin(),
		       segment_filter_adjustments.end() );

  boost::hash_range( hash_val, map.begin(), map.end() );

  return hash_val;
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
