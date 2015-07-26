#include "continuation_state.hh"

using namespace std;

ContinuationState::ContinuationState( const uint16_t width, const uint16_t height )
  : prev_references_( width, height ),
    last_output_( prev_references_.last ),
    shown_( false ),
    on_key_frame_( true ),
    key_frame_(),
    inter_frame_()
{}

ContinuationState::ContinuationState( KeyFrame && frame, bool shown, const RasterHandle & raster,
                                      const References & refs )
  : prev_references_( refs ),
    last_output_( raster ),
    shown_( shown ),
    on_key_frame_( true ),
    key_frame_( move( frame ) ),
    inter_frame_()
{}

ContinuationState::ContinuationState( InterFrame && frame,bool shown, const RasterHandle & raster,
                                      const References & refs )
  : prev_references_( refs ),
    last_output_( raster ),
    shown_( shown ),
    on_key_frame_( false ),
    key_frame_(),
    inter_frame_( move( frame ) )
{}

SerializedFrame ContinuationState::make_continuation( const DecoderDiff & difference ) const
{
  // Should only be making continuations off a shown frame
  assert( shown_ );
  if ( on_key_frame_ ) {
    SourceHash source_hash = difference.get_source_hash( key_frame_.get().get_used() );
    TargetHash target_hash = difference.get_target_hash( key_frame_.get().get_updated(),
                                                         last_output_, shown_ );

    return SerializedFrame( key_frame_.get().serialize( difference.target_probabilities ),
                            source_hash, target_hash, get_shown() );
  }
  else {
    bool last_missing = difference.source_refs.last != prev_references_.last;
    bool golden_missing = difference.source_refs.golden != prev_references_.golden;
    bool alt_missing = difference.source_refs.alternative_reference != prev_references_.alternative_reference;

    InterFrame continuation_frame( inter_frame_.get(), difference.continuation_diff,
                                   last_missing, golden_missing, alt_missing,
                                   difference.entropy_header, difference.filter_update, difference.segment_update );

    continuation_frame.optimize_continuation_coefficients();

    SourceHash source_hash = difference.get_source_hash( continuation_frame.get_used() );
    TargetHash target_hash = difference.get_target_hash( continuation_frame.get_updated(),
                                                         last_output_, shown_ );

    return SerializedFrame( continuation_frame.serialize( difference.target_probabilities ),
                            source_hash, target_hash, get_shown() );
  }
}

void ContinuationState::apply_last_frame( Decoder & source, const Decoder & target ) const
{
  if ( on_key_frame_ ) {
    source.apply_decoded_frame( key_frame_.get(), last_output_, target );
  }
  else {
    source.apply_decoded_frame( inter_frame_.get(), last_output_, target );
  }
}

bool ContinuationState::is_shown( void ) const
{
  return shown_;
}

RasterHandle ContinuationState::get_output( void ) const
{
  return last_output_;
}

Optional<RasterHandle> ContinuationState::get_shown( void ) const
{
  return make_optional( shown_, last_output_ );
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

UpdateTracker ContinuationState::get_frame_updated( void ) const
{
  if ( on_key_frame_ ) {
    return key_frame_.get().get_updated();
  }
  else {
    return inter_frame_.get().get_updated();
  }
}
