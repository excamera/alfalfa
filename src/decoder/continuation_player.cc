#include "continuation_player.hh"
#include "uncompressed_chunk.hh"

using namespace std;

string PreContinuation::continuation_name( void ) const
{
  return source_hash.str() + "#" + target_hash.str();
}

ContinuationPlayer::ContinuationPlayer( const uint16_t width, const uint16_t height )
  : FramePlayer( width, height ),
    prev_references_( width, height ),
    shown_( false ),
    cur_output_( prev_references_.last ),
    cur_updates_( false, false, false, false, false, false, false ),
    on_key_frame_( true ),
    key_frame_(),
    inter_frame_()
{}

template<class FrameType>
Optional<RasterHandle> ContinuationPlayer::track_continuation_info( const FrameType & frame )
{
  prev_references_ = decoder_.get_references();

  tie( shown_, cur_output_ ) = decoder_.decode_frame( frame );

  return make_optional( shown_, cur_output_ );
}

Optional<RasterHandle> ContinuationPlayer::decode( const SerializedFrame & serialized_frame )
{
  UncompressedChunk decompressed_frame = decoder_.decompress_frame( serialized_frame.chunk() );
  cur_updates_ = serialized_frame.get_target_hash();

  if ( decompressed_frame.key_frame() ) {
    on_key_frame_ = true;
    key_frame_ = decoder_.parse_frame<KeyFrame>( decompressed_frame );

    return track_continuation_info( key_frame_.get() );
  } else {
    on_key_frame_ = false;
    inter_frame_ = decoder_.parse_frame<InterFrame>( decompressed_frame );

    return track_continuation_info( inter_frame_.get() );
  }
}

void ContinuationPlayer::apply_changes( Decoder & other ) const
{
  if ( on_key_frame_ ) {
    other.apply_decoded_frame( key_frame_.get(), cur_output_, decoder_ );
  }
  else {
    other.apply_decoded_frame( inter_frame_.get(), cur_output_, decoder_ );
  }
}

bool ContinuationPlayer::need_gen_continuation( void ) const
{
  return not on_key_frame_;
}

PreContinuation ContinuationPlayer::make_pre_continuation( const SourcePlayer & source ) const
{
  // The first continuation needs to be unshown, because it is outputting an upgraded
  // image of the same frame. If it is shown, graph generation will skip over it,
  // because it is looking for shown frames for the next frame, not the current one
  bool show_continuation = not source.first_continuation();

  assert( not on_key_frame_ );
  DependencyTracker used = inter_frame_.get().get_used();
  MissingTracker missing = source.decoder_.find_missing( prev_references_ );

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

  return PreContinuation { show_continuation, missing, source.decoder_.source_hash( used ),
                           decoder_.target_hash( cur_updates_, cur_output_, show_continuation ) };
}

SerializedFrame ContinuationPlayer::make_continuation( const PreContinuation & pre,
                                                       const SourcePlayer & source ) const
{
  DecoderDiff difference = decoder_ - source.decoder_;

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
                          make_optional( pre.shown, cur_output_ ) );
}

SerializedFrame ContinuationPlayer::operator-( const SourcePlayer & source ) const
{
  return make_continuation( make_pre_continuation( source ), source );
}
