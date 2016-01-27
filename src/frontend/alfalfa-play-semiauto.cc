#include <cstdlib>
#include <iostream>
#include <chrono>

#include <sysexits.h>

#include "alfalfa_video_client.hh"
#include "display.hh"
#include "frame_fetcher.hh"
#include "video_map.hh"

using namespace std;
using namespace std::chrono;
using AbridgedFrameInfo = AlfalfaProtobufs::AbridgedFrameInfo;

int main( const int argc, char const *argv[] )
{
  if ( argc != 3 ) {
    cerr << "Usage: " << argv[ 0 ] << " <server-address> <track>" << "\n";
    return EX_USAGE;
  }

  const AlfalfaVideoClient video { argv[ 1 ] };
  Decoder decoder( video.get_video_width(), video.get_video_height() );
  VideoDisplay display { decoder.example_raster() };

  FrameFetcher fetcher { video.get_url() };
  VideoMap video_map { argv[ 1 ] };
  
  const size_t track_to_play = stoi( argv[ 2 ] );

  /* start playing */
  auto next_raster_time = steady_clock::now();
  bool playing = false;
  unsigned int frames_played = 0;
  const unsigned int frames_to_play = video_map.track_length_full( track_to_play );
  auto last_feasibility_analysis = steady_clock::now();
  unsigned int analysis_generation = video_map.analysis_generation();
  auto current_future_of_track = video_map.track_snapshot( track_to_play, frames_played );

 while ( frames_played < frames_to_play ) {
    /* kick off a feasibility analysis? */
    const auto now = steady_clock::now();
    if ( now - last_feasibility_analysis > milliseconds( 250 ) ) {
      video_map.update_annotations( fetcher.estimated_bytes_per_second() * 0.8,
				    fetcher.frame_db_snapshot() );
      last_feasibility_analysis = now;
    }

    /* is a new analysis available? */
    const unsigned int new_analysis_generation = video_map.analysis_generation();
    if ( new_analysis_generation != analysis_generation ) {
      current_future_of_track = video_map.track_snapshot( track_to_play, frames_played );
      fetcher.set_frame_plan( current_future_of_track );
      analysis_generation = new_analysis_generation;
    }
    
    /* are we out of available track? */
    if ( current_future_of_track.empty() ) {
      this_thread::sleep_until( next_raster_time );
      continue;
    }
    
    /* should we resume from a stall? */
    if ( not playing ) {
      fetcher.wait_until_feasible();
      playing = true;
      next_raster_time = steady_clock::now();
      cerr << "Playing.\n";
    }

    /* do we need to stall? */
    if ( not fetcher.has_frame( current_future_of_track.front() ) ) {
      /* stall */
      cerr << "Stalling.\n";
      playing = false;
      continue;
    }

    const string coded_frame = fetcher.coded_frame( current_future_of_track.front() );
    current_future_of_track.pop_front();
    const Optional<RasterHandle> raster = decoder.parse_and_decode_frame( coded_frame );
    if ( raster.initialized() ) {
      this_thread::sleep_until( next_raster_time );
      display.draw( raster.get() );
      next_raster_time += microseconds( lrint( 1000000.0 / 24.0 ) );
    }

    frames_played++;
  }

  return EXIT_SUCCESS;
}
