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

  deque<AnnotatedFrameInfo> current_future_of_track;
  AnnotatedFrameInfo last_frame_decoded = VideoMap::no_frame();

  const auto frame_interval = microseconds( lrint( 1000000.0 / 24.0 ) );
  
  while ( rasters_displayed < rasters_to_display ) {
    /* kick off a feasibility analysis? */
    const auto now = steady_clock::now();
    if ( now - last_feasibility_analysis > milliseconds( 500 ) ) {
      video_map.update_annotations( fetcher.estimated_bytes_per_second() * 0.75,
				    fetcher.frame_db_snapshot() );
      last_feasibility_analysis = now;
    }

    /* is a new analysis available? */
    const unsigned int new_analysis_generation = video_map.analysis_generation();
    if ( new_analysis_generation != analysis_generation ) {
      current_future_of_track = video_map.best_plan( last_frame_decoded, playing );
      fetcher.set_frame_plan( current_future_of_track );
      analysis_generation = new_analysis_generation;
      //      video_map.report_feasibility();
      //      cerr << "kilobits per second: " << fetcher.estimated_bytes_per_second() * 8 * 0.8 / 1000.0 << "\n";
    }

    /* should we fetch a switch? */
    if ( not playing ) {
      video_map.fetch_switch( last_frame_decoded.track_id,
			      last_frame_decoded.track_index,
			      5 ); /* XXX */
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
      if ( fetcher.is_plan_feasible() ) {
	playing = true;
	next_raster_time = steady_clock::now();
	cerr << "Playing.\n";
      } else {
	this_thread::sleep_for( 4 * frame_interval );
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

    /* pop the frame from the plan and apply it */
    last_frame_decoded = current_future_of_track.front(); 
    current_future_of_track.pop_front();
    const string coded_frame = fetcher.coded_frame( last_frame_decoded );

    const Optional<RasterHandle> raster = decoder.parse_and_decode_frame( coded_frame );
    if ( raster.initialized() ) {
      this_thread::sleep_until( next_raster_time );
      display.draw( raster.get() );
      next_raster_time += frame_interval;
      rasters_displayed++;
    }

    if ( rasters_displayed == 10 * 24 ) {
      cerr << "Seeking!\n\n\n";
      rasters_displayed = 60 * 24 * 3;
      last_frame_decoded.track_id = 6;
      last_frame_decoded.track_index = rasters_displayed;
      current_future_of_track.clear();
    }
  }

  return EXIT_SUCCESS;
}
