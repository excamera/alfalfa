#include "dependency_tracking.hh"
#include "exception.hh"

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>

using namespace std;

static double median( const vector<unsigned> & container )
{
  vector<unsigned> sorted_container( container );
  sort( sorted_container.begin(), sorted_container.end() );

  if ( sorted_container.size() % 2 == 0 ) {
    return (double)( sorted_container[ sorted_container.size() / 2 - 1 ] +
                     sorted_container[ sorted_container.size() / 2 ] ) / 2;
  }
  else {
    return sorted_container[ sorted_container.size() / 2 ];
  }
}

static double mean( const vector<unsigned> & container )
{
  double total = 0;
  for ( unsigned elem : container ) {
    total += elem;
  }

  return total / container.size();
}

struct Frame {
  SourceHash source;
  TargetHash target;
  unsigned size;
  string name;

  Frame( const string & name, unsigned size )
    : source( name ),
      target( name ),
      size( size ),
      name( name )
  {}
};

class FrameManager
{
private:
  unordered_map<string, const Frame *> name_to_frame_;
  vector<Frame> frames_;

public:
  FrameManager( const vector<Frame> & frames )
    :  name_to_frame_(),
       frames_( frames )
  {
    for ( const Frame & frame : frames_ ) {
      name_to_frame_[ frame.name ] = &frame;
    }
  }

  const Frame * frame_for_name( const string & frame_name ) const
  {
    auto iter = name_to_frame_.find( frame_name );
    if ( iter == name_to_frame_.end() ) {
      cout << frame_name << " ruh roh\n";
    }
    return iter->second;
  }

};

class StreamTracker {
private:
  vector<unsigned> key_frame_sizes_;
  vector<unsigned> key_frame_nums_;
  unsigned num_key_frames_;
  unsigned total_frames_;
  unsigned total_displayed_frames_;
  unsigned frames_per_second_;

  vector<const Frame *> stream_frames_;

  const FrameManager & frame_manager_;

  vector<DecoderHash> decoders_;
  vector<size_t> outputs_;
  vector<unsigned> shown_indexes_;

public:
  StreamTracker( const vector<string> & stream_frames, const FrameManager & frame_manager )
    : key_frame_sizes_(),
      key_frame_nums_(),
      num_key_frames_( 0 ),
      total_frames_( stream_frames.size() ),
      total_displayed_frames_( 0 ),
      frames_per_second_( 24 ),
      stream_frames_(),
      frame_manager_( frame_manager ),
      decoders_(),
      outputs_(),
      shown_indexes_()
  {
    DecoderHash decoder( 0, 0, 0, 0, 0 );

    for ( const string & frame_name : stream_frames ) {
      const Frame * frame = frame_manager_.frame_for_name( frame_name );
      stream_frames_.push_back( frame );

      assert( decoder.can_decode( frame->source ) );
      decoder.update( frame->target );
      decoders_.push_back( decoder );
      outputs_.push_back( frame->target.output_hash );

      if ( frame->target.shown ) {
        shown_indexes_.push_back( decoders_.size() - 1 );
        total_displayed_frames_++;

        // Is this a keyframe
        if ( not frame->source.state_hash.initialized() and not frame->source.continuation_hash.initialized() and
             not frame->source.last_hash.initialized() and not frame->source.golden_hash.initialized() and
             not frame->source.alt_hash.initialized() ) {
          num_key_frames_++;
          key_frame_sizes_.push_back( frame->size );
          key_frame_nums_.push_back( total_displayed_frames_ );
        }
      }
    }
  }

  unsigned total_frames( void ) const { return total_frames_; }

  double total_time( void ) const
  {
    return (double)total_displayed_frames_ / frames_per_second_;
  }

  double mean_keyframe_interval( void ) const
  {
    vector<unsigned> keyframe_intervals;

    for ( unsigned i = 1; i < key_frame_nums_.size(); i++ ) {
      keyframe_intervals.push_back( key_frame_nums_[ i ] - key_frame_nums_[ i - 1 ] );
    }
    assert( keyframe_intervals.size() == num_key_frames_ - 1 );

    return mean( keyframe_intervals ) / frames_per_second_;
  }

  double mean_keyframe_size( void ) const
  {
    return mean( key_frame_sizes_ );
  }

  double median_keyframe_size( void ) const
  {
    return median( key_frame_sizes_ );
  }

  unsigned total_stream_size( void ) const
  {
    unsigned total = 0;
    for ( const Frame * frame : stream_frames_ ) {
      total += frame->size;
    }
    return total;
  }

  vector<unsigned> switch_sizes( const StreamTracker & target ) const
  {
    vector<unsigned> switch_sizes;
    vector<vector<const Frame *>> all_switch_frames;
    for ( unsigned switch_num = 0; switch_num < total_displayed_frames_; switch_num++ ) {
      vector<const Frame *> switch_frames;
      unsigned switch_size = 0;

      DecoderHash source_decoder( decoders_[ shown_indexes_[ switch_num ] ] );

      unsigned cur_displayed = switch_num;
      unsigned target_index = target.shown_indexes_[ cur_displayed ];

      bool shown_continuation = false;

      while ( cur_displayed < total_displayed_frames_ ) {
        const Frame * target_frame = target.stream_frames_[ target_index ];
        DecoderHash target_decoder( target.decoders_[ target_index ] );

        if ( source_decoder == target_decoder ) {
          break;
        }

        if ( source_decoder.can_decode( target_frame->source ) ) {
          source_decoder.update( target_frame->target );
          switch_frames.push_back( target_frame );
        } else {
          // Need continuation
          // Jump to the next displayed frame in the target
          target_index = target.shown_indexes_[ cur_displayed ];
          target_frame = target.stream_frames_[ target_index ];
          assert(target_frame->target.shown);

          SourceHash new_source = target_frame->source;
          TargetHash new_target = target_frame->target;
          new_target.shown = shown_continuation;

          bool continuation_depend = false;

          if ( new_source.last_hash.initialized() and new_source.last_hash.get() != source_decoder.last_hash() ) {
            new_source.last_hash.clear();
            continuation_depend = true;
          }
          if ( new_source.golden_hash.initialized() and new_source.golden_hash.get() != source_decoder.golden_hash() ) {
            new_source.golden_hash.clear();
            continuation_depend = true;
          }
          if ( new_source.alt_hash.initialized() and new_source.alt_hash.get() != source_decoder.alt_hash() ) {
            new_source.alt_hash.clear();
            continuation_depend = true;
          }

          if ( continuation_depend ) {
            new_source.continuation_hash = source_decoder.continuation_hash();
          }
          new_source.state_hash = make_optional( true, source_decoder.state_hash() );


          string continuation_frame_name = new_source.str() + "#" + new_target.str();

          const Frame * continuation = frame_manager_.frame_for_name( continuation_frame_name );
          if ( continuation_depend ) {
            // Only track continuations depending on the preloop raster
            switch_size += continuation->size;
          }

          assert( source_decoder.can_decode( continuation->source ) );
          source_decoder.update( continuation->target );
          switch_frames.push_back( continuation );
        }

        cur_displayed++;
        target_index++;
        shown_continuation = true;
      }

      if ( switch_size != 0 ) {
        switch_sizes.push_back( switch_size );
        all_switch_frames.push_back( switch_frames );
      }
    }

    for ( unsigned i = 0; i < switch_sizes.size(); i++ ) {
      cerr << "Switch size " << switch_sizes[ i ] << endl;
      for ( auto frame : all_switch_frames[ i ] ) {
        cerr << frame->name << " " << frame->size << endl;
      }
    }

    return switch_sizes;
  }
};

vector<unsigned> best_switch_sizes( const vector<unsigned> & switch_sizes, unsigned interval )
{
  vector<unsigned> best;
  for (unsigned start_range = 0; start_range < switch_sizes.size() - interval; start_range++ ) {
    unsigned best_switch = switch_sizes[ start_range ];
    for ( unsigned i = 1; i < interval; i++ ) {
      unsigned switch_size = switch_sizes[ start_range + i ];
      if ( switch_size < best_switch ) {
        best_switch = switch_size;
      }
    }
    best.push_back( best_switch );
  }

  return best;
}

int main( int argc, char * argv[] )
{
  if ( argc < 2 ) {
    cerr << "Usage: " << argv[ 0 ] << " VIDEO_DIR\n";
    return EXIT_FAILURE;
  }

  if ( chdir( argv[ 1 ] ) ) {
    throw unix_error( "chdir failed" );
  }

  ifstream frame_manifest( "frame_manifest" );
  ifstream stream_manifest( "stream_manifest" );

  vector<Frame> frames;
  while ( not frame_manifest.eof() ) {
    string frame_name;
    unsigned frame_size;
    frame_manifest >> frame_name >> frame_size;
    if ( frame_name == "" ) {
      break;
    }

    frames.emplace_back( frame_name, frame_size );
  }

  FrameManager frame_manager( frames );

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

  vector<StreamTracker> streams;
  for ( auto & frames : stream_frames ) {
    streams.emplace_back( frames, frame_manager );
  }

  cout << fixed;
  for ( unsigned stream_idx = 0; stream_idx < streams.size(); stream_idx++ ) {
    cout << "Stream " << stream_idx << ":\n";

    const StreamTracker & stream = streams[ stream_idx ];

    cout << "Length: " << stream.total_time() << " seconds\n";
    cout << "Total frames: " << stream.total_frames() << "\n";
    cout << "Bitrate: " << (stream.total_stream_size() * 8 / stream.total_time()) / 1000 << " kilobits per second\n";
    cout << "KeyFrame frequency: " << stream.mean_keyframe_interval() << " seconds between keyframes\n";
    cout << "KeyFrame median size: " << stream.median_keyframe_size() << " bytes\n";
    cout << "KeyFrame mean size: " << stream.mean_keyframe_size() << " bytes\n";

    cout << endl;
  }

  for ( unsigned stream_idx = 0; stream_idx < streams.size() - 1; stream_idx++ ) {
    const StreamTracker & source = streams[ stream_idx ];
    const StreamTracker & target = streams[ stream_idx + 1 ];

    vector<unsigned> switch_sizes = source.switch_sizes( target );
    vector<unsigned> best_sizes = best_switch_sizes( switch_sizes, 48 );

    cout << "Stream " << stream_idx << " -> Stream " << stream_idx + 1 << ":\n";
    cout << "Switch median size: " << median( switch_sizes ) << " bytes\n";
    cout << "Switch mean size: " << mean( switch_sizes ) << " bytes\n";
    cout << "Best 2 sec switch median size: " << median( best_sizes ) << " bytes\n";
    cout << "Best 2 sec switch mean size: " << mean( best_sizes ) << " bytes\n\n";

    switch_sizes = target.switch_sizes( source );
    best_sizes = best_switch_sizes( switch_sizes, 48 );

    cout << "Stream " << stream_idx + 1 << " -> Stream " << stream_idx << ":\n";
    cout << "Switch median size: " << median( switch_sizes ) << " bytes\n";
    cout << "Switch mean size: " << mean( switch_sizes ) << " bytes\n";
    cout << "Best 2 sec switch median size: " << median( best_sizes ) << " bytes\n";
    cout << "Best 2 sec switch mean size: " << mean( best_sizes ) << " bytes\n\n";
  }
}
