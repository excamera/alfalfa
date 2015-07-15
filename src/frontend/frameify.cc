#include <iostream>
#include <fstream>
#include <unordered_set>
#include <algorithm>
#include "continuation_player.hh"

using namespace std;

// Streams keep track of all the players converging onto them,
// because they need to be able to update the states of those
// frames with their continuation frames
class StreamState {
private:
  ContinuationPlayer stream_player_;

  // Reference to the FrameGenerator's manifest
  ofstream & frame_manifest_;

  vector<FramePlayer> continuation_players_ {};

  Optional<SerializedFrame> last_shown_frame_ {};

  void update_continuation_players( const SerializedFrame & frame )
  {
    for ( FramePlayer & player : continuation_players_ ) {
      if ( player.can_decode( frame ) ) {
        player.decode( frame );
      }
    }

    // Don't need to track players which have fully converged
    remove( continuation_players_.begin(), continuation_players_.end(), stream_player_ );
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

      if ( frame.shown() ) {
        // FIXME this is ugly
        if ( last_shown_frame_.initialized() ) {
          last_shown_frame_.clear();
        }
        last_shown_frame_.initialize( frame );
        return frame;
      }

      // For unshown frames, update the continuation players, shown
      // frames will be accounted for when continuation frames are made
      update_continuation_players( frame );
      update_continuation_players( frame );

    }
    throw Unsupported( "Undisplayed frames at end of video unsupported" );
  }

  void add_continuation( const StreamState & other )
  {
    continuation_players_.emplace_back( other.stream_player_ );
  }

  void make_continuations( void )
  {
    // If a source player has converged on target_player remove it from consideration
    // for further continuation frames
    remove( continuation_players_.begin(), continuation_players_.end(), stream_player_ );

    for ( FramePlayer & source_player : continuation_players_ ) {
      if ( source_player.can_decode( last_shown_frame_.get() ) ) {
        source_player.decode( last_shown_frame_.get() );
      }
      else {
        SerializedFrame continuation = stream_player_ - source_player; 

        write_frame( continuation );

        // FIXME same as above fixme
        source_player.decode( continuation );
      }
    }
  }
};

class FrameGenerator {
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

        target_stream.add_continuation( source_stream );
        source_stream.add_continuation( target_stream );
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
