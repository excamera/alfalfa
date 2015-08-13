#include <iostream>
#include <fstream>
#include <unordered_set>
#include <algorithm>
#include "continuation_player.hh"

using namespace std;

class SourcePlayer : public FramePlayer
{
private:
  bool need_continuation_;

public:
  SourcePlayer( const ContinuationPlayer & original )
    : FramePlayer( original ),
      need_continuation_( true ) // Assume we're making a continuation frame unless we've shown otherwise
  {}

  void set_need_continuation( bool b )
  {
    need_continuation_ = b;
  }

  bool need_continuation( void ) const
  {
    return need_continuation_;
  }

  void sync_changes( const ContinuationPlayer & target_player )
  {
    target_player.apply_changes( decoder_ );
  }
};

// Streams keep track of all the players converging onto them,
// because they need to be able to update the states of those
// frames with their continuation frames
class StreamState
{
private:
  ContinuationPlayer stream_player_;

  // Reference to the manifest stored in FrameGenerator
  ofstream & frame_manifest_;

  // List of players we are generating continuations from
  vector<SourcePlayer> source_players_ {};

  // Shared set of all the frames generated so far
  static unordered_set<string> frame_names_;

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
        player.sync_changes( stream_player_ );
        player.set_need_continuation( false );
      }
      else {
        player.set_need_continuation( true );
      }
    }
  }

  void write_frame( const SerializedFrame & frame )
  {
    string frame_name = frame.name();

    // FIXME, ideally we could guarantee that write_frame was never
    // called with dups and could avoid this check
    if ( not frame_names_.count( frame_name ) ) {
      frame_names_.insert( frame_name );
      frame_manifest_ << frame_name << " " << frame.size() << endl;
      frame.write();
    }
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
        // FIXME this isn't taking advantage of RasterDiff...
        SerializedFrame continuation = stream_player_ - source_player; 

        write_frame( continuation );

        assert( source_player.can_decode( continuation ) );
        // Sync the most recent changes from stream player (the same effect as decoding continuation would have)
        source_player.sync_changes( stream_player_ );
      }
    }
  }
};
unordered_set<string> StreamState::frame_names_ = unordered_set<string>();

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
