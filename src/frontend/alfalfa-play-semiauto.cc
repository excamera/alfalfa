#include <cstdlib>
#include <iostream>

#include <sysexits.h>

#include "alfalfa_video_client.hh"
#include "display.hh"
#include "frame_fetcher.hh"

using namespace std;
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
  const size_t track_size = 2000; // video.get_track_size( track_to_play );

  cerr << "Getting the track list... ";
  const auto abridged_track = video.get_abridged_frames( track_to_play,
							 0, track_size );
  cerr << "done.\n";

  /* start fetching */
  vector<AbridgedFrameInfo> track;
  track.insert( track.begin(), abridged_track.frame().begin(), abridged_track.frame().end() );
  fetcher.set_frame_plan( track );

  /* start playing */
  for ( const auto & x : track ) {
    const string coded_frame = fetcher.wait_for_frame( x );
    const Optional<RasterHandle> raster = decoder.parse_and_decode_frame( coded_frame );
    if ( raster.initialized() ) { display.draw( raster.get() ); }
  }

  return EXIT_SUCCESS;
}
