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

  const size_t group_size = 24;

  for ( size_t begin_frame = 0; begin_frame + group_size < track_size; begin_frame += group_size ) {
    vector<AbridgedFrameInfo> frames_to_fetch;
    frames_to_fetch.insert( frames_to_fetch.begin(),
			    abridged_track.frame().begin() + begin_frame,
			    abridged_track.frame().begin() + begin_frame + group_size );
    const vector<string> coded_frames = fetcher.get_chunks( frames_to_fetch );
    
    for ( const auto & coded_frame : coded_frames ) {
      const Optional<RasterHandle> raster = decoder.parse_and_decode_frame( coded_frame );
      if ( raster.initialized() ) {
	display.draw( raster.get() );
      }
    }
  }
  
  return EXIT_SUCCESS;
}
