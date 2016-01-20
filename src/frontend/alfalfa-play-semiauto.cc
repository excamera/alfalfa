#include <cstdlib>
#include <iostream>

#include <sysexits.h>

#include "alfalfa_video_client.hh"
#include "display.hh"
#include "frame_fetcher.hh"

using namespace std;

int main( const int argc, char const *argv[] )
{
  if ( argc != 3 ) {
    cerr << "Usage: " << argv[ 0 ] << " <server-address> <framestore-url>" << "\n";
    return EX_USAGE;
  }

  const AlfalfaVideoClient video { argv[ 1 ] };
  Decoder decoder( video.get_video_width(), video.get_video_height() );
  VideoDisplay display { decoder.example_raster() };

  FrameFetcher fetcher { argv[ 2 ] };
  
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
  
  const size_t track_to_play = track_ids.front();
  const size_t track_size = video.get_track_size( track_to_play );
  const vector<FrameInfo> frames = video.get_frames( track_to_play, 0, track_size );

  for ( const auto & frame : frames ) {
    const string coded_frame = video.get_chunk( frame );
    const Optional<RasterHandle> raster = decoder.parse_and_decode_frame( coded_frame );
    if ( raster.initialized() ) {
      display.draw( raster.get() );
    }
  }
  
  return EXIT_SUCCESS;
}
