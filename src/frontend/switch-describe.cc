#include "dependency_tracking.hh"
#include "exception.hh"
#include "alfalfa_video.hh"

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>

using namespace std;

template <class ContainerType>
static double median( const ContainerType & container )
{
  ContainerType sorted_container( container );
  sort( sorted_container.begin(), sorted_container.end() );

  if ( sorted_container.size() % 2 == 0 ) {
    return (double)( sorted_container[ sorted_container.size() / 2 - 1 ] +
                     sorted_container[ sorted_container.size() / 2 ] ) / 2;
  }
  else {
    return sorted_container[ sorted_container.size() / 2 ];
  }
}

template <class ContainerType>
static double mean( const ContainerType & container )
{
  double total = 0;
  for ( auto elem : container ) {
    total += elem;
  }

  return total / container.size();
}

class StreamTracker {
private:
  const PlayableAlfalfaVideo & alf_;

  vector<unsigned> key_frame_sizes_;
  vector<unsigned> key_frame_nums_;
  unsigned num_key_frames_;
  unsigned total_frames_;
  vector<TrackDBIterator> displayed_frames_;
  unsigned total_displayed_frames_;
  unsigned frames_per_second_;
  double stream_size_;

  bool is_key_frame( const FrameInfo & frame )
  {
    UncompressedChunk uncompressed( alf_.get_chunk( frame ), alf_.get_info().width, alf_.get_info().height );

    return uncompressed.key_frame();
  }

  bool is_experimental_frame( const FrameInfo & frame )
  {
    UncompressedChunk uncompressed( alf_.get_chunk( frame ), alf_.get_info().width, alf_.get_info().height );

    return uncompressed.experimental();
  }

public:
  StreamTracker( const TrackDBIterator & start, const TrackDBIterator & end, const PlayableAlfalfaVideo & alf )
    : alf_( alf ),
      key_frame_sizes_(),
      key_frame_nums_(),
      num_key_frames_( 0 ),
      total_frames_( 0 ),
      displayed_frames_(),
      total_displayed_frames_( 0 ),
      frames_per_second_( 24 ),
      stream_size_( 0 )
  {
    for ( TrackDBIterator cur = start; cur != end; cur++ ) {
      const FrameInfo & frame = *cur;
      if ( frame.shown() ) {
        displayed_frames_.push_back( cur );
        total_displayed_frames_++;
      }
      if ( is_key_frame( frame ) ) {
        num_key_frames_++;
        key_frame_sizes_.push_back( frame.length() );
        key_frame_nums_.push_back( total_frames_ );
      }

      stream_size_ += frame.length();
      total_frames_++;
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

  double total_stream_size( void ) const
  {
    return stream_size_;
  }

  vector<double> switch_sizes( const StreamTracker & target ) const
  {
    vector<double> switch_sizes;
    for ( unsigned cur_switch = 0; cur_switch < total_displayed_frames_; cur_switch++ ) {
      double switch_size = 0;
      double target_size = 0;
      TrackDBIterator source_pos = displayed_frames_[ cur_switch ];
      TrackDBIterator target_pos = target.displayed_frames_[ cur_switch ];

      auto switches = alf_.get_frames( source_pos, target_pos.track_id() );

      for ( SwitchDBIterator s = switches.first; s != switches.second; s++ ) {
        switch_size += s->length();
      }

      TrackDBIterator target_end = alf_.get_frames( switches.second ).first;
      for ( TrackDBIterator t = target_pos; t != target_end; t++ ) {
        target_size += t->length();
      }

      switch_sizes.push_back( switch_size - target_size );
    }

    return switch_sizes;
  }
};

vector<double> best_switch_sizes( const vector<double> & switch_sizes, unsigned interval )
{
  vector<double> best;
  for (unsigned start_range = 0; start_range < switch_sizes.size() - interval; start_range++ ) {
    double best_switch = switch_sizes[ start_range ];
    for ( unsigned i = 1; i < interval; i++ ) {
      double switch_size = switch_sizes[ start_range + i ];
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
    cerr << "Usage: " << argv[ 0 ] << " Input alf\n";
    return EXIT_FAILURE;
  }

  PlayableAlfalfaVideo alf( argv[ 1 ] );

  auto tracks = alf.get_track_ids();

  vector<StreamTracker> streams;
  for ( auto track = tracks.first; track != tracks.second; track++ ) {
    auto track_bounds = alf.get_frames( *track );
    streams.emplace_back( track_bounds.first, track_bounds.second, alf );
  }

  cout << fixed;
  for ( unsigned stream_idx = 0; stream_idx < streams.size(); stream_idx++ ) {
    cout << "Track " << stream_idx << ":\n";

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

    auto switch_sizes = source.switch_sizes( target );
    auto best_sizes = best_switch_sizes( switch_sizes, 48 );

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
