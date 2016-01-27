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
  if ( argc != 2 ) {
    cerr << "Usage: " << argv[ 0 ] << " <server-address>" << "\n";
    return EX_USAGE;
  }

  const AlfalfaVideoClient video { argv[ 1 ] };
  Decoder decoder( video.get_video_width(), video.get_video_height() );
  VideoDisplay display { decoder.example_raster() };

  FrameFetcher fetcher { video.get_url() };

  const unsigned int rasters_to_display = video.get_raster_count();
  
  VideoMap video_map { argv[ 1 ], rasters_to_display };

  /* start playing */
  auto next_raster_time = steady_clock::now();
  bool playing = false;
  unsigned int rasters_displayed = 0;
  auto last_feasibility_analysis = steady_clock::now();
  unsigned int analysis_generation = video_map.analysis_generation();

  unsigned int last_track_played = 0;
  unsigned int last_frame_index = -1;

  deque<AnnotatedFrameInfo> current_future_of_track;

  const auto frame_interval = microseconds( lrint( 1000000.0 / 24.0 ) );
  
  while ( rasters_displayed < rasters_to_display ) {
    /* kick off a feasibility analysis? */
    const auto now = steady_clock::now();
    if ( now - last_feasibility_analysis > milliseconds( 100 ) ) {
      video_map.update_annotations( fetcher.estimated_bytes_per_second() * 0.8,
				    fetcher.frame_db_snapshot() );
      last_feasibility_analysis = now;
    }

    /* is a new analysis available? */
    const unsigned int new_analysis_generation = video_map.analysis_generation();
    if ( new_analysis_generation != analysis_generation ) {
      current_future_of_track = video_map.best_plan( last_track_played, last_frame_index + 1, playing );
      fetcher.set_frame_plan( current_future_of_track );
      analysis_generation = new_analysis_generation;
      //      video_map.report_feasibility();
      //      cerr << "kilobits per second: " << fetcher.estimated_bytes_per_second() * 8 * 0.8 / 1000.0 << "\n";
    }
    
    /* are we out of available track? */
    if ( current_future_of_track.empty() ) {
      cerr << "Stalling [out of track]\n";
      playing = false;
      this_thread::sleep_for( frame_interval );
      continue;
    }
    
    /* should we resume from a stall? */
    if ( not playing ) {
      if ( current_future_of_track.front().time_margin_required <= 0 ) {
	playing = true;
	next_raster_time = steady_clock::now();
	cerr << "Playing.\n";
      } else {
	this_thread::sleep_for( frame_interval );
	continue;
      }
    }

    /* do we need to stall? */
    if ( not fetcher.has_frame( current_future_of_track.front() ) ) {
      /* stall */
      cerr << "Stalling.\n";
      playing = false;
      continue;
    }

    const string coded_frame = fetcher.coded_frame( current_future_of_track.front() );
    last_track_played = current_future_of_track.front().track_id;
    last_frame_index = current_future_of_track.front().track_index;

    if ( rasters_displayed > 60 * 24 ) {
      last_track_played = 6;
      last_frame_index = 60 * 24; /* XXX */
    }
    
    current_future_of_track.pop_front();
    cerr << last_track_played << " ";
    const Optional<RasterHandle> raster = decoder.parse_and_decode_frame( coded_frame );
    if ( raster.initialized() ) {
      this_thread::sleep_until( next_raster_time );
      display.draw( raster.get() );
      next_raster_time += frame_interval;
      rasters_displayed++;
    }
  }

  return EXIT_SUCCESS;
}
