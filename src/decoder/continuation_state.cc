#include "continuation_state.hh"

using namespace std;

string PreContinuation::continuation_name( void ) const
{
  return source_hash.str() + "#" + target_hash.str();
}

ContinuationState::ContinuationState( const uint16_t width, const uint16_t height )
  : prev_references_( width, height ),
    last_output_( prev_references_.last ),
    cur_updates_( false, false, false, false, false, false, false ),
    shown_( false ),
    on_key_frame_( true ),
    key_frame_(),
    inter_frame_()
{}

ContinuationState::ContinuationState( KeyFrame && frame, bool shown, const RasterHandle & raster,
                                      const UpdateTracker & updates, const References & refs )
  : prev_references_( refs ),
    last_output_( raster ),
    cur_updates_( updates ),
    shown_( shown ),
    on_key_frame_( true ),
    key_frame_( move( frame ) ),
    inter_frame_()
{}

ContinuationState::ContinuationState( InterFrame && frame,bool shown, const RasterHandle & raster,
                                      const UpdateTracker & updates, const References & refs )
  : prev_references_( refs ),
    last_output_( raster ),
    cur_updates_( updates ),
    shown_( shown ),
    on_key_frame_( false ),
    key_frame_(),
    inter_frame_( move( frame ) )
{}

PreContinuation ContinuationState::make_pre_continuation( const Decoder & target, const Decoder & source,
                                                          const bool shown ) const
{
  assert( not on_key_frame_ );
  DependencyTracker used = inter_frame_.get().get_used();
  MissingTracker missing = source.find_missing( prev_references_ );

  if ( missing.last and used.need_last ) {
    used.need_last = false;
    used.need_continuation = true;
  }

  if ( missing.golden and used.need_golden ) {
    used.need_golden = false;
    used.need_continuation = true;
  }

  if ( missing.alternate and used.need_alternate ) {
    used.need_alternate = false;
    used.need_continuation = true;
  }

  return PreContinuation { shown, missing, source.source_hash( used ), target.target_hash( cur_updates_, last_output_, shown ) };
}

SerializedFrame ContinuationState::make_continuation( const DecoderDiff & difference, const PreContinuation & pre ) const
{
  // Should only be making continuations off a shown frame
  assert( shown_ );
  // A key frame will have been written out anyway and can be used by anyone
  assert( not on_key_frame_ );
  InterFrame continuation_frame( inter_frame_.get(), difference.continuation_diff, pre.shown,
                                 pre.missing.last, pre.missing.golden, pre.missing.alternate,
                                 difference.entropy_header, difference.filter_update, difference.segment_update );

  continuation_frame.optimize_continuation_coefficients();

  return SerializedFrame( continuation_frame.serialize( difference.target_probabilities ),
                          pre.source_hash, pre.target_hash,
                          make_optional( pre.shown, last_output_ ) );
  
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

bool ContinuationState::on_key_frame( void ) const
{
  return on_key_frame_;
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
  return cur_updates_;
}
