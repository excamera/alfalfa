#include "decoder.hh"
#include "uncompressed_chunk.hh"
#include "frame.hh"
#include "decoder_state.hh"
#include "diff_generator.hh"

#include <sstream>

using namespace std;

Decoder::Decoder( const uint16_t width, const uint16_t height )
  : state_( width, height ),
    references_( width, height )
{}

Optional<RasterHandle> Decoder::decode_frame( const Chunk & frame )
{
  /* parse uncompressed data chunk */
  UncompressedChunk uncompressed_chunk( frame, state_.width, state_.height );

  /* get a RasterHandle */
  MutableRasterHandle raster { state_.width, state_.height };
  bool shown;

  if ( uncompressed_chunk.key_frame() ) {
    const KeyFrame myframe = state_.parse_and_apply<KeyFrame>( uncompressed_chunk );

    shown = myframe.show_frame();

    myframe.decode( state_.segmentation, raster );

    references_.update_continuation( raster );

    myframe.loopfilter( state_.segmentation, state_.filter_adjustments, raster );

    RasterHandle immutable_raster( move( raster ) );

    myframe.copy_to( immutable_raster, references_ );

    return Optional<RasterHandle>( shown, immutable_raster );
  } else {
    const InterFrame myframe = state_.parse_and_apply<InterFrame>( uncompressed_chunk );

    shown = myframe.show_frame();

    myframe.decode( state_.segmentation, references_, raster );

    references_.update_continuation( raster );

    myframe.loopfilter( state_.segmentation, state_.filter_adjustments, raster );

    RasterHandle immutable_raster( move( raster ) );

    myframe.copy_to( immutable_raster, references_ );

    return Optional<RasterHandle>( shown, immutable_raster );
  }
}

string Decoder::hash_str( const ReferenceTracker & used_refs,
				  const References & references ) const
{
  stringstream decoder_hash;
  decoder_hash << hex << uppercase;

  decoder_hash << state_.hash() << "_";

  if ( used_refs.continuation() ) {
    decoder_hash << references.continuation.get().hash();
  } else {
    decoder_hash << "x";
  }
  decoder_hash << "_";

  if ( used_refs.last() ) {
    decoder_hash << references.last.get().hash();
  } else {
    decoder_hash << "x";
  }
  decoder_hash << "_";

  if ( used_refs.golden() ) {
    decoder_hash << references.golden.get().hash();
  } else {
    decoder_hash << "x";
  }
  decoder_hash << "_";

  if ( used_refs.alternate() ) {
    decoder_hash << references.alternative_reference.get().hash();
  } else {
    decoder_hash << "x";
  }

  return decoder_hash.str();
}

string Decoder::hash_str( const ReferenceTracker & used_refs ) const
{
  return hash_str( used_refs, references_ );
}

string Decoder::hash_str( void ) const
{
  /* All references set to used so partial_hash hashes every reference */

  return hash_str( ReferenceTracker( true, true, true, true ) );
}

bool Decoder::equal_references( const Decoder & other ) const
{
  if ( references_.continuation.get() == other.references_.continuation.get() ) {
    cout << "Continuation refs equal\n";
  }

  return references_.continuation.get() == other.references_.continuation.get() and
    references_.last.get() == other.references_.last.get() and
    references_.golden.get() == other.references_.golden.get() and
    references_.alternative_reference.get() == other.references_.alternative_reference.get();
}

void Decoder::update_continuation( const Decoder & other )
{
  references_.continuation = other.references_.continuation;
}

bool Decoder::operator==( const Decoder & other ) const
{
  return state_ == other.state_ and
    references_.continuation.get() == other.references_.continuation.get();
}

References::References( const uint16_t width, const uint16_t height )
  : References( MutableRasterHandle( width, height ) )
{}

References::References( MutableRasterHandle && raster )
  : last( move( raster ) ),
    golden( last ),
    alternative_reference( last ),
    continuation( last )
{}

void References::update_continuation( const MutableRasterHandle & raster )
{
  MutableRasterHandle copy_raster( raster.get().display_width(), raster.get().display_height() );

  copy_raster.get().copy_from( raster );
  continuation = RasterHandle( move( copy_raster ) );
}

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

size_t ProbabilityTables::hash( void ) const
{
  size_t hash_val = 0;

  for ( auto const & block_sub : coeff_probs ) {
    for ( auto const & bands_sub : block_sub ) {
      for ( auto const & contexts_sub : bands_sub ) {
	boost::hash_range( hash_val, contexts_sub.begin(), contexts_sub.end() );
      }
    }
  }

  boost::hash_range( hash_val, y_mode_probs.begin(), y_mode_probs.end() );

  boost::hash_range( hash_val, uv_mode_probs.begin(), uv_mode_probs.end() );

  for ( auto const & sub : motion_vector_probs ) {
    boost::hash_range( hash_val, sub.begin(), sub.end() );
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
