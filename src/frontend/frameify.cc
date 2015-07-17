#include <iostream>
#include <fstream>
#include <unordered_set>
#include <algorithm>
#include "continuation_player.hh"

using namespace std;

class SourcePlayer : public FramePlayer
{
private:
  const ContinuationPlayer * orig_player_;
  bool need_continuation_;

public:
  SourcePlayer( const ContinuationPlayer & original )
    : FramePlayer( original ),
      orig_player_( &original ),
      need_continuation_( true ) // Assume we're making a continuation frame unless we've shown otherwise
  {}

  SourcePlayer( const SourcePlayer & other )
    : FramePlayer( other ),
      orig_player_( other.orig_player_ ),
      need_continuation_( true )
  {}

  SourcePlayer & operator=( const SourcePlayer & other )
  {
    FramePlayer::operator=( other );
    orig_player_ = other.orig_player_;
    need_continuation_ = other.need_continuation_;

    return *this;
  }

  void set_need_continuation( bool b )
  {
    need_continuation_ = b;
  }

  bool need_continuation( void )
  {
    return need_continuation_;
  }

  void sync_continuation_raster( void )
  {
  // This hugely complicates graph generation and doesn't seem to have a positive impact
  // tested cq8 cq9
#if 0
    FramePlayer::sync_continuation_raster( *orig_player_ );
#endif
  }

  bool operator==( const SourcePlayer & other ) const
  {
    return FramePlayer::operator==( other ) and need_continuation_ == other.need_continuation_ and orig_player_ == other.orig_player_;
  }
};

// Streams keep track of all the players converging onto them,
// because they need to be able to update the states of those
// frames with their continuation frames
class StreamState
{
private:
  ContinuationPlayer stream_player_;

  // Reference to the FrameGenerator's manifest
  ofstream & frame_manifest_;

  vector<SourcePlayer> source_players_ {};

  // Remove any players that have converged onto stream_player
  // and any duplicates
  void cleanup_source_players( void ) {
    vector<SourcePlayer> new_players {};
    new_players.reserve( source_players_.size() );

    for ( SourcePlayer & player : source_players_ ) {
      if ( player != stream_player_ && find( new_players.begin(), new_players.end(), player ) == new_players.end() ) {
        new_players.emplace_back( move( player ) );
      }
    }

    source_players_ = move( new_players );
  }

  void update_source_players( const SerializedFrame & frame )
  {
    for ( SourcePlayer & player : source_players_ ) {
      if ( player.can_decode( frame ) ) {
        //FIXME it would probably be faster to emulate this since all the necessary information is
        // in the stream_player's
        player.decode( frame );
        player.set_need_continuation( false );
      }
      else {
        player.set_need_continuation( true );
      }
    }
  }

  void write_frame( const SerializedFrame & frame )
  {
    frame_manifest_ << frame.name() << " " << frame.size() << endl;
    frame.write();
  }


public:
  StreamState( const string & stream_file, ofstream & frame_manifest )
    : stream_player_( stream_file ),
      frame_manifest_( frame_manifest )
  {}

  SerializedFrame advance_until_shown( void )
  {
    /* Serialize and write until next displayed frame */
    while ( not stream_player_.eof() ) {
      SerializedFrame frame = stream_player_.serialize_next(); 
      write_frame( frame );

      update_source_players( frame );

      if ( frame.shown() ) {
        return frame;
      }
    }
    throw Unsupported( "Undisplayed frames at end of video unsupported" );
  }

  void new_source( StreamState & other )
  {
    source_players_.emplace_back( other.stream_player_ );
  }

  void make_continuations( void )
  {
    cleanup_source_players();

    for ( SourcePlayer & source_player : source_players_ ) {
      if ( source_player.need_continuation() ) {
        source_player.sync_continuation_raster();

        // FIXME this isn't taking advantage of RasterDiff...
        SerializedFrame continuation = stream_player_ - source_player; 

        frame_manifest_ << "continuation\n";
        write_frame( continuation );

        assert( source_player.can_decode( continuation ) );
        // FIXME same as above
        source_player.decode( continuation );
      }
    }
  }
};

class FrameGenerator
{
private: 

  ofstream original_manifest_ { "original_manifest" };
  ofstream quality_manifest_ { "quality_manifest" };
  ofstream frame_manifest_ { "frame_manifest" };
  vector<StreamState> streams_ {};

  Player original_player_;

  void record_quality( const SerializedFrame & frame, const RasterHandle & original )
  {
    quality_manifest_ << hex << uppercase << original.hash() << " " << 
                frame.get_output().hash() << " " << frame.psnr( original ) << endl;
  }

  void record_original( const RasterHandle & original )
  {
    original_manifest_ << uppercase << hex << original.hash() << endl;
  }

public:
  FrameGenerator( const string & original, const char * const streams[] )
    : original_player_( original )
  {
    original_manifest_ << original_player_.width() << " " << original_player_.height() << "\n";

    // Argument list is null terminated
    for ( int i = 0; streams[ i ] != nullptr; i++ ) {
      streams_.emplace_back( streams[ i ], frame_manifest_ );
    }
  }

  void generate( void )
  {
    while ( not original_player_.eof() ) {
      RasterHandle original_raster = original_player_.advance();
      record_original( original_raster );

      for ( StreamState & stream : streams_ ) {
        SerializedFrame shown_frame =  stream.advance_until_shown();
        assert( shown_frame.shown() );

        record_quality( shown_frame, original_raster );
      }

      // Add new starting points for continuations
      for ( unsigned stream_idx = 0; stream_idx < streams_.size() - 1; stream_idx++ ) {
        StreamState & source_stream = streams_[ stream_idx ];
        StreamState & target_stream = streams_[ stream_idx + 1 ];

        // Insert new copies of source_player and target_players for continuations starting
        // at this frame

        target_stream.new_source( source_stream );

        source_stream.new_source( target_stream );
      }

      // Make the continuations for each stream
      for ( StreamState & stream : streams_ ) {
        stream.make_continuations();
      }
    }
  }
};

int main( int argc, const char * const argv[] )
{
  if ( argc < 5 ) {
    cerr << "Usage: " << argv[ 0 ] << " OUTPUT_DIR ORIGINAL STREAM1 STREAM2 [ MORE_STREAMS ]" << endl;
    return EXIT_FAILURE;
  }

  if ( mkdir( argv[ 1 ], 0777 ) ) {
    throw unix_error( "mkdir failed" );
  }
  if ( chdir( argv[ 1 ] ) ) {
    throw unix_error( "chdir failed" );
  }
  FrameGenerator generator( argv[ 2 ], argv + 3 );
  generator.generate();
}
