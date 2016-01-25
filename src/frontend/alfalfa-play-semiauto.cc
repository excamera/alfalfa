#include <cstdlib>
#include <iostream>
#include <chrono>

#include <sysexits.h>

#include "alfalfa_video_client.hh"
#include "display.hh"
#include "frame_fetcher.hh"

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
  
  /* print out the available tracks in the video */
  const vector<size_t> track_ids = video.get_track_ids();

  cout << "track_ids:";
  for ( const auto & id : track_ids ) {
    cout << " " << id;
  }
  cout << "\n";

  /* play the first track found */
  if ( track_ids.empty() ) {
    throw runtime_error( "no tracks to play" );
  }

  const size_t track_to_play = stoi( argv[ 2 ] );
  const size_t track_size = video.get_track_size( track_to_play );

  cerr << "Getting the track list... ";
  const auto abridged_track = video.get_abridged_frames( track_to_play,
							 0, track_size );
  cerr << "done.\n";

  /* start fetching */
  fetcher.set_frame_plan( abridged_track.frame() );

  /* start playing */
  auto next_raster_time = steady_clock::now();
  bool playing = false;
  auto frame_it = abridged_track.frame().begin();
  while ( true ) {
    if ( frame_it == abridged_track.frame().end() ) {
      break; /* done with video */
    }

    /* should we resume from a stall? */
    if ( not playing ) {
      fetcher.wait_until_feasible();
      playing = true;
      next_raster_time = steady_clock::now();
      cerr << "Playing.\n";
    }

    /* do we need to stall? */
    if ( not fetcher.has_frame( *frame_it ) ) {
      /* stall */
      cerr << "Stalling.\n";
      playing = false;
      continue;
    }
    
    const string coded_frame = fetcher.coded_frame( *frame_it );
    const Optional<RasterHandle> raster = decoder.parse_and_decode_frame( coded_frame );
    if ( raster.initialized() ) {
      this_thread::sleep_until( next_raster_time );
      display.draw( raster.get() );
      next_raster_time += microseconds( lrint( 1000000.0 / 24.0 ) );
    }

    frame_it++;
  }

  return EXIT_SUCCESS;
}
