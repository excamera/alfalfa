#include <iostream>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>
#include <limits>
#include "continuation_player.hh"

using namespace std;

// Streams keep track of all the players converging onto them,
// because they need to be able to update the states of those
// frames with their continuation frames
class StreamState
{
private:
  ContinuationPlayer stream_player_;

  // Reference to the manifest stored in ContinuationGenerator
  ofstream & frame_manifest_;

  // List of players we are generating continuations from
  vector<SourcePlayer> source_players_ {};

  // Shared set of all the frames generated so far
  static unordered_set<string> frame_names_;

  vector<string> stream_frames_;
  unsigned cur_frame_;

  // Remove any players that have converged onto stream_player
  // and any duplicates
  void cleanup_source_players( void )
  {
    vector<SourcePlayer> new_players;
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
  StreamState( const vector<string> & stream_frames, ofstream & frame_manifest,
               const uint16_t width, const uint16_t height )
    : stream_player_( width, height ),
      frame_manifest_( frame_manifest ),
      stream_frames_( stream_frames ),
      cur_frame_( 0 )
  {}

  SerializedFrame advance_until_shown( void )
  {
    /* Serialize and write until next displayed frame */
    while ( cur_frame_ < stream_frames_.size() ) {
      SerializedFrame frame( stream_frames_[ cur_frame_ ] );
      cur_frame_++;

      stream_player_.decode( frame );
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
      // Check if the source needs a continuation at this point
      if ( source_player.need_continuation() ) {
        // Only make a continuation if the player is on an interframe
        // (keyframes are free switches, so by checking explicitly here the
        // continuation code can ignore the possibility of a keyframe
        if ( stream_player_.need_gen_continuation() ) {
          PreContinuation pre = stream_player_.make_pre_continuation( source_player );
          // Serializing these continuations is *very* expensive, so check if we already have the necessary continuation frame
          if ( not frame_names_.count( pre.continuation_name() ) ) {
            // FIXME this isn't taking advantage of RasterDiff...
            
            SerializedFrame continuation =
              stream_player_.make_continuation( pre, source_player ); 

            write_frame( continuation );

            assert( source_player.can_decode( continuation ) );
          }
        }

        // Even if we didn't need to make a new continuation frame, we still need to sync the most recent
        // changes from stream player (the same effect as decoding stream_player's last frame)
        source_player.sync_changes( stream_player_ );
      }
    }
  }

  bool eos( void ) const
  {
    return cur_frame_ == stream_frames_.size();
  }
};
unordered_set<string> StreamState::frame_names_ = unordered_set<string>();

class ContinuationGenerator
{
private: 
  // Frameify already wrote out the "normal" frames to the manifest,
  // so open frame_manifest in append mode
  ofstream frame_manifest_ { "frame_manifest", ios_base::app };

  vector<StreamState> streams_ {};

  unsigned cur_displayed_frame_ { 0 };

  template<bool output_all>
  void output_continuations( const unordered_set<unsigned> & switch_nums )
  {
    while ( not streams_[ 0 ].eos() ) {
      for ( StreamState & stream : streams_ ) {
        stream.advance_until_shown();
      }

      if ( output_all or switch_nums.count( cur_displayed_frame_ ) ) {
        // Add new starting points for continuations
        for ( unsigned stream_idx = 0; stream_idx < streams_.size() - 1; stream_idx++ ) {
          StreamState & source_stream = streams_[ stream_idx ];
          StreamState & target_stream = streams_[ stream_idx + 1 ];

          // Insert new copies of source_player and target_players for continuations starting
          // at this frame

          target_stream.new_source( source_stream );

          source_stream.new_source( target_stream );
        }
      }

      // Make the continuations for each stream
      for ( StreamState & stream : streams_ ) {
        stream.make_continuations();
      }

      cur_displayed_frame_++;
    }
  }

public:
  ContinuationGenerator( void )
  {
    ifstream original_manifest( "original_manifest" );
    ifstream stream_manifest( "stream_manifest" );

    uint16_t width, height;
    original_manifest >> width >> height;

    vector<vector<string>> stream_frames;

    while ( not stream_manifest.eof() ) {
      unsigned stream_idx;
      string frame_name;
      stream_manifest >> stream_idx >> frame_name;
      if ( frame_name == "" ) {
        break;
      }

      if ( stream_idx >= stream_frames.size() ) {
        stream_frames.resize( stream_idx + 1 );
      }
      stream_frames[ stream_idx ].push_back( frame_name );
    }

    for ( const vector<string> & frames : stream_frames ) {
      streams_.emplace_back( frames, frame_manifest_, width, height );
    }
  }

  void output_all_continuations( void )
  {
    output_continuations<true>( unordered_set<unsigned>() );
  }

  void output_specific_continuations( const unordered_set<unsigned> & switch_nums )
  {
    output_continuations<false>( switch_nums );
  }
};

int main( int argc, const char * const argv[] )
{
  if ( argc < 3 ) {
    cerr << "Usage: " << argv[ 0 ] << " VIDEO_DIR FRAMES ..." << endl;
    return EXIT_FAILURE;
  }

  if ( chdir( argv[ 1 ] ) ) {
    throw unix_error( "chdir failed" );
  }

  ContinuationGenerator generator;

  if ( string( argv[ 2 ] ) == "all" ) {
    generator.output_all_continuations();
  }
  else {
    unordered_set<unsigned> switch_nums;
    for ( int i = 2; i < argc; i++ ) {
      switch_nums.insert( stoul( argv[ i ] ) );
    }
    generator.output_specific_continuations( switch_nums );
  }
}
