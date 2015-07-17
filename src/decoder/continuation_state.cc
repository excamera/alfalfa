#include "continuation_state.hh"

using namespace std;

ContinuationState::ContinuationState( const uint16_t width, const uint16_t height )
  : prev_references_( width, height ),
    last_shown_(),
    on_key_frame_( true ),
    key_frame_(),
    inter_frame_()
{}

ContinuationState::ContinuationState( KeyFrame && frame, const Optional<RasterHandle> & raster,
                                      const References & refs )
  : prev_references_( refs ),
    last_shown_( raster ),
    on_key_frame_( true ),
    key_frame_( move( frame ) ),
    inter_frame_()
{}

ContinuationState::ContinuationState( InterFrame && frame, const Optional<RasterHandle> & raster,
                                      const References & refs )
  : prev_references_( refs ),
    last_shown_( raster ),
    on_key_frame_( false ),
    key_frame_(),
    inter_frame_( move( frame ) )
{}

SerializedFrame ContinuationState::make_continuation( const DecoderDiff & difference ) const
{
  // Should only be making continuations off a shown frame
  assert( last_shown_.initialized() );
  if ( on_key_frame_ ) {
    DecoderHash source_hash = difference.source_hash.filter( key_frame_.get().get_used() );
    DecoderHash target_hash = difference.target_hash.filter( key_frame_.get().get_updated() );

    return SerializedFrame( key_frame_.get().serialize( difference.target_probabilities ),
                            source_hash, target_hash, last_shown_ );
  }
  else {
    bool last_missing = prev_references_.last.hash() != difference.source_hash.last_ref().get();
    bool golden_missing = prev_references_.golden.hash() != difference.source_hash.golden_ref().get();
    bool alt_missing = prev_references_.alternative_reference.hash() != difference.source_hash.alt_ref().get();

    InterFrame continuation_frame( inter_frame_.get(), difference.continuation_diff,
                                   last_missing, golden_missing, alt_missing,
                                   difference.entropy_header, difference.filter_update, difference.segment_update );

    continuation_frame.optimize_continuation_coefficients();

    DecoderHash source_hash = difference.source_hash.filter( continuation_frame.get_used() );
    DecoderHash target_hash = difference.target_hash.filter( continuation_frame.get_updated() );

    return SerializedFrame( continuation_frame.serialize( difference.target_probabilities ),
                            source_hash, target_hash, last_shown_ );
  }
}

Optional<RasterHandle> ContinuationState::get_shown( void ) const
{
  return last_shown_;
}

string ContinuationState::get_frame_stats( void ) const
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

DependencyTracker ContinuationState::get_frame_used( void ) const
{
  if ( on_key_frame_ ) {
    return key_frame_.get().get_used();
  }
  else {
    return inter_frame_.get().get_used();
  }
}

DependencyTracker ContinuationState::get_frame_updated( void ) const
{
  if ( on_key_frame_ ) {
    return key_frame_.get().get_updated();
  }
  else {
    return inter_frame_.get().get_updated();
  }
}
