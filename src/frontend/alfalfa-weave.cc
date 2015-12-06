#include <iostream>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>
#include <limits>
#include "continuation_player.hh"
#include "alfalfa_video.hh"

using namespace std;

class SwitchPlayer : public SourcePlayer
{
private:
  struct Switch {
    TrackDBIterator switch_start;
    vector<FrameInfo> frames;
  };

  vector<Switch> switches_;

public:
  SwitchPlayer( const TrackDBIterator & start, const ContinuationPlayer & original )
    : SourcePlayer( original ),
      switches_ { Switch { start, {} } }
  {}

  void add_switch_frame( const FrameInfo & frame )
  {
    for ( Switch & switch_ : switches_ ) {
      switch_.frames.push_back( frame );
    }
  }

  void merge( const SwitchPlayer & other )
  {
    for ( const Switch & sw : other.switches_ ) {
      switches_.emplace_back( move( sw ) );
    }
  }

  void write_switches( WritableAlfalfaVideo & alf, const TrackDBIterator & switch_end ) const
  {
    for ( const Switch & switch_ : switches_ ) {
      alf.insert_switch_frames( switch_.switch_start, switch_.frames, switch_end );
    }
  }
};

// Streams keep track of all the players converging onto them,
// because they need to be able to update the states of those
// frames with their continuation frames
class StreamState
{
private:
  ContinuationPlayer stream_player_;

  vector<FrameInfo> cur_frames_ {};
  // List of players we are generating continuations from
  vector<SwitchPlayer> switch_players_ {};

  TrackDBIterator track_frame_;
  TrackDBIterator track_end_;
  
  const PlayableAlfalfaVideo & source_alf_;

  // Remove any players that have converged onto stream_player
  // and any duplicates
  void cleanup_switch_players( WritableAlfalfaVideo & new_alf )
  {
    vector<SwitchPlayer> new_players;
    new_players.reserve( switch_players_.size() );

    for ( SwitchPlayer & player : switch_players_ ) {
      if ( player == stream_player_ ) {
        // If this player has converged onto the track, we no longer
        // need to track it, so add its info to the SwitchDB
        player.write_switches( new_alf, ++TrackDBIterator( track_frame_ ) );
      } else {
        auto iter = find( new_players.begin(), new_players.end(), player ); 
        if ( iter != new_players.end() ) {
          // If two players are equal but have yet to converge
          // we only need to track one copy, but merge together their switch
          // information
          iter->merge( player );
        } else {
          new_players.emplace_back( move( player ) );
        }
      }
    }

    switch_players_ = move( new_players );
  }

public:
  StreamState( const pair<TrackDBIterator, TrackDBIterator> & track_pair,
               const PlayableAlfalfaVideo & source_alf )
    : stream_player_( source_alf.get_info().width, source_alf.get_info().height ),
      track_frame_( track_pair.first ),
      track_end_( track_pair.second ),
      source_alf_( source_alf )
  {}

  void update_switch_players( WritableAlfalfaVideo & alf )
  {
    for ( SwitchPlayer & player : switch_players_ ) {
      for ( const FrameInfo & frame : cur_frames_ ) {
        if ( not player.need_continuation() and player.can_decode( frame ) ) {
          player.safe_decode( frame, source_alf_.get_chunk( frame ) );
          player.add_switch_frame( frame );
        } else {
          player.set_need_continuation( true );
        }
      }
    }

    cleanup_switch_players( alf );
  }


  void advance_until_shown()
  {
    cur_frames_.clear();
    // Serialize and write until next displayed frame
    while ( not eos() ) {
      const FrameInfo & frame = *track_frame_;

      stream_player_.decode( source_alf_.get_chunk( frame ) );
      cur_frames_.push_back( frame );

      if ( frame.shown() ) {
        return;
      }

      track_frame_++;
    }
    throw Unsupported( "Undisplayed frames at end of video unsupported" );
  }

  void advance_past_shown()
  {
    track_frame_++;
  }

  void new_source( StreamState & other )
  {
    switch_players_.emplace_back( other.track_frame_, other.stream_player_ );
  }

  // Create all the necessary continuation frames for this point in the track,
  // and save them into new_alf
  void make_continuations( WritableAlfalfaVideo & new_alf )
  {
    for ( SwitchPlayer & switch_player : switch_players_ ) {
      // Check if the source needs a continuation at this point
      if ( switch_player.need_continuation() ) {
        for ( const SerializedFrame & frame : stream_player_.make_reference_updates( switch_player ) ) {
          FrameInfo info = new_alf.import_serialized_frame( frame );
          switch_player.add_switch_frame( info );
          switch_player.safe_decode( info, frame.chunk() );
        }

        if ( stream_player_.need_state_update( switch_player ) ) {
          SerializedFrame state_update = stream_player_.make_state_update( switch_player );
          FrameInfo info = new_alf.import_serialized_frame( state_update );
          switch_player.add_switch_frame( info );
          switch_player.safe_decode( info, state_update.chunk() );
        }

        SerializedFrame inter = stream_player_.rewrite_inter_frame( switch_player ); 
        FrameInfo info = new_alf.import_serialized_frame( inter );
        switch_player.add_switch_frame( info );
        
        // Even if we didn't need to make a new continuation frame, we still need to sync the most recent
        // changes from stream player (the same effect as decoding stream_player's last frame)
        //stream_player_.apply_changes( switch_player );
        switch_player.safe_decode( info, inter.chunk() );

        // Set player as not needing continuation, will be reevaluated next frame
        switch_player.set_need_continuation( false );
      }
    }
  }

  void write_final_switches( WritableAlfalfaVideo & new_alf )
  {
    for ( const SwitchPlayer & player : switch_players_ ) {
      player.write_switches( new_alf, track_end_ );
    }
  }

  bool eos( void ) const
  {
    return track_frame_ == track_end_;
  }
};

class ContinuationGenerator
{
private: 
  PlayableAlfalfaVideo orig_alf_;
  WritableAlfalfaVideo new_alf_;
  vector<StreamState> streams_ {};

public:
  ContinuationGenerator( const string & source_alf_path, const string & new_alf_path,
                         vector<size_t> track_ids )
    : orig_alf_( source_alf_path ), new_alf_( new_alf_path, orig_alf_.get_info().fourcc, orig_alf_.get_info().width, orig_alf_.get_info().height )
  {
    /* Copy in the source video */
    combine( new_alf_, orig_alf_ );

    for ( size_t track_id : track_ids ) {
      streams_.emplace_back( orig_alf_.get_frames( track_id ), orig_alf_ );
    }
  }

  void write_continuations()
  {
    while ( not streams_[ 0 ].eos() ) {
      for ( StreamState & stream : streams_ ) {
        stream.advance_until_shown();
      }

      // Add new starting points for continuations
      for ( unsigned stream_idx = 0; stream_idx < streams_.size() - 1; stream_idx++ ) {
        StreamState & source_stream = streams_[ stream_idx ];
        StreamState & target_stream = streams_[ stream_idx + 1 ];

        // Insert new copies of switch_player and target_players for continuations starting
        // at this frame

        target_stream.new_source( source_stream );

        source_stream.new_source( target_stream );
      }

      for ( StreamState & stream : streams_ ) {
        stream.update_switch_players( new_alf_ );
        stream.make_continuations( new_alf_ );
        stream.advance_past_shown();
      }
    }

    for ( StreamState & stream : streams_ ) {
      stream.write_final_switches( new_alf_ );
    }

    new_alf_.save();
  }
};

int main( int argc, const char * const argv[] )
{
  if ( argc < 5 ) {
    cerr << "Usage: " << argv[ 0 ] << " <input-alf> <output-alf> <trajectoryA> <trajectoryB> ..." << endl;
    return EXIT_FAILURE;
  }

  FileSystem::create_directory( argv[ 2 ] );

  vector<size_t> track_ids;
  for ( int i = 3; i < argc; i++ ) {
    track_ids.push_back( stoul( argv[ i ] ) );
  }

  ContinuationGenerator generator( argv[ 1 ], argv[ 2 ], track_ids );

  generator.write_continuations();
}
