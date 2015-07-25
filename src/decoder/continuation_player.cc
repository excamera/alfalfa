#include "continuation_player.hh"

using namespace std;

ContinuationPlayer::ContinuationPlayer( const string & file_name )
  : FilePlayer( file_name ),
    continuation_state_( width(), height() )
{}

SerializedFrame ContinuationPlayer::serialize_next( void )
{
  Chunk frame = get_next_frame();

  // Save the source so we can hash the parts of it that are used by
  // the next frame;
  Decoder source = decoder_;

  continuation_state_ = decoder_.next_continuation_state( frame );

  SourceHash source_hash = source.source_hash( continuation_state_.get_frame_used() );
  TargetHash target_hash = decoder_.target_hash( continuation_state_.get_frame_updated(),
                                                 continuation_state_.get_output(),
                                                 continuation_state_.is_shown() );

  // Check this is a sane frame
  assert( source.get_hash().can_decode( source_hash ) );
  assert( decoder_.get_hash().continuation_hash() == target_hash.continuation_hash );

  return SerializedFrame( frame, source_hash, target_hash, continuation_state_.get_shown() );
}

RasterHandle ContinuationPlayer::next_output( void )
{
  continuation_state_ = decoder_.next_continuation_state( get_next_frame() );
  return continuation_state_.get_output();
}

// Override FilePlayer's version so we track continuation_state_
RasterHandle ContinuationPlayer::advance( void )
{
  while ( not eof() ) {
    continuation_state_ = decoder_.next_continuation_state( get_next_frame() );
    if ( continuation_state_.get_shown().initialized() ) {
      return continuation_state_.get_shown().get();
    }
  }

  throw Unsupported( "hidden frames at end of file" );
}

string ContinuationPlayer::get_frame_stats( void ) const
{
  return continuation_state_.get_frame_stats();
}

SerializedFrame ContinuationPlayer::operator-( const FramePlayer & source ) const
{
  DecoderDiff difference = decoder_difference( source );
  return continuation_state_.make_continuation( difference );
}
