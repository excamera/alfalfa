#include "continuation_player.hh"

using namespace std;

ContinuationPlayer::ContinuationPlayer( const string & file_name )
  : FilePlayer( file_name ),
    continuation_state_( width(), height() )
{}

SerializedFrame ContinuationPlayer::serialize_next( void )
{
  Chunk frame = get_next_frame();

  // DecoderHash is a light wrapper around SafeArray
  DecoderHash source_hash = decoder_.get_hash();

  continuation_state_ = decoder_.next_continuation_state( frame );

  source_hash = source_hash.filter( continuation_state_.get_frame_used() );
  DecoderHash target_hash = decoder_.get_hash().filter( continuation_state_.get_frame_updated() );

  return SerializedFrame( frame, source_hash, target_hash, continuation_state_.get_shown() );
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
