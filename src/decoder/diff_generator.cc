#include "diff_generator.hh"
#include "uncompressed_chunk.hh"
#include "decoder_state.hh"

#include <sstream>

using namespace std;

DiffGenerator::DiffGenerator( const uint16_t width, const uint16_t height )
  : Decoder( width, height ),
    on_key_frame_( true ),
    key_frame_(),
    inter_frame_(),
    prev_references_( width, height )
{}

/* Since frames are uncopyable, only copy the base */
DiffGenerator::DiffGenerator( const DiffGenerator & other )
  : Decoder ( other ),
    on_key_frame_ ( false ),
    key_frame_(),
    inter_frame_(),
    prev_references_( other.prev_references_ )
{}

Optional<RasterHandle> DiffGenerator::decode_frame( const Chunk & frame )
{
  /* parse uncompressed data chunk */
  UncompressedChunk uncompressed_chunk( frame, state_.width, state_.height );

  /* get a RasterHandle */
  /* XXX will become MutableRasterHandle */
  RasterHandle raster { state_.width, state_.height };
  bool shown;

  if ( uncompressed_chunk.key_frame() ) {
    on_key_frame_ = true;

    key_frame_ = state_.parse_and_apply<KeyFrame>( uncompressed_chunk );

    shown = key_frame_.get().show_frame();

    key_frame_.get().decode( state_.segmentation, raster );

    // Store copy of the raster for diffs before loopfilter is applied
    references_.update_continuation( raster );

    key_frame_.get().loopfilter( state_.segmentation, state_.filter_adjustments, raster );

    key_frame_.get().copy_to( raster, references_ );
  } else {
    on_key_frame_ = false;

    inter_frame_ = state_.parse_and_apply<InterFrame>( uncompressed_chunk );

    shown = inter_frame_.get().show_frame();

    inter_frame_.get().decode( state_.segmentation, references_, raster );

    references_.update_continuation( raster );

    /* Only need to copy prev_references for inter frames, since keyframes don't depend on them,
     * also copy after update_continuation so prev_references_ has the latest preloop for
     * diff creation */
    prev_references_ = references_;

    inter_frame_.get().loopfilter( state_.segmentation, state_.filter_adjustments, raster );

    inter_frame_.get().copy_to( raster, references_ );
  }

  return Optional<RasterHandle>( shown, move( raster ) );
}

string DiffGenerator::cur_frame_stats( void ) const
{
  if ( on_key_frame_ ) {
    return "\tKeyFrame\n";
  }
  else {
    string stats = "\tInterFrame\n" + inter_frame_.get().stats(); 
    if ( inter_frame_.get().continuation_header().initialized() ) {
      ContinuationHeader c_hdr = inter_frame_.get().continuation_header().get();
      stats += "\t";
      if ( c_hdr.missing_last_frame ) {
	stats += "Missing Last ";
      }
      if ( c_hdr.missing_golden_frame ) {
	stats += "Missing Gold ";
      }
      if ( c_hdr.missing_alternate_reference_frame ) {
	stats += "Missing ALT ";
      }
      stats += "\n";
    }
    return stats;
  }
}

string DiffGenerator::source_hash_str( const Decoder & source ) const
{
  if ( on_key_frame_ ) {
    return "x_x_x_x_x";
  }
  else {
    return source.hash_str( inter_frame_.get().used_references() );
  }
}

ReferenceTracker DiffGenerator::updated_references( void ) const
{
  if ( on_key_frame_ ) {
    return key_frame_.get().updated_references();
  }
  else {
    return inter_frame_.get().updated_references();
  }
}

string DiffGenerator::target_hash_str( void ) const
{
  return hash_str( updated_references() );
}

void DiffGenerator::set_references( bool set_last, bool set_golden, bool set_alt,
                                    const DiffGenerator & other )
{
  if ( set_last ) {
    references_.last = other.references_.last;
  }

  if ( set_golden ) {
    references_.golden = other.references_.golden;
  }

  if ( set_alt ) {
    references_.alternative_reference = other.references_.alternative_reference;
  }
}

ReferenceTracker DiffGenerator::missing_references( const DiffGenerator & other ) const
{
  ReferenceTracker missing( true, true, true, true );

  if ( references_.last.get() == other.references_.last.get() ) {
    missing.set_last( false );
  }
  if ( references_.golden.get() == other.references_.golden.get() ) {
    missing.set_golden( false );
  }
  if ( references_.alternative_reference.get() == 
       other.references_.alternative_reference.get() ) {
    missing.set_alternate( false );
  }

  return missing;
}

// This needs to be made const, which means rewriting rewrite_as_diff into make_continuation_frame
SerializedFrame DiffGenerator::operator-( const DiffGenerator & source_decoder ) const
{
  if ( on_key_frame_ ) {
    return SerializedFrame( key_frame_.get().serialize( state_.probability_tables ),
			    true, "x_x_x_x_x", hash_str() );
  } else {
    InterFrame diff_frame( inter_frame_.get(), source_decoder.state_, state_,
			   source_decoder.references_, prev_references_ );

    diff_frame.optimize_continuation_coefficients();

    return SerializedFrame( diff_frame.serialize( state_.probability_tables ), true,
			    source_decoder.hash_str( diff_frame.used_references() ),
			    hash_str( diff_frame.updated_references() ) );
  }
}
