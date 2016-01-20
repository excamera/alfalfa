#include <iostream>
#include <sstream>

#include "player.hh"
#include "exception.hh"
#include "alfalfa_video_client.hh"
#include "manifests.hh"
#include "frame_db.hh"
#include "frame_fetcher.hh"

using namespace std;

int main( int argc, char *argv[] )
{
  try {
    if ( argc != 3 ) {
      cerr << "Usage: " << argv[ 0 ] << " <server-adress> <track-id>" << endl;
      return EXIT_FAILURE;
    }

    string server_address( argv[ 1 ] );
    AlfalfaVideoClient alf_client( server_address );
    FrameFetcher web( alf_client.get_url() );

    // Use string stream to convert track_id string to a size_t
    stringstream type_converter( argv[ 2 ] );
    size_t track_id;
    type_converter >> track_id;

    // Get the track iterators from the alfalfa video for the specified track
    // A track iterator can be used to find the frame info objects for a track
    size_t track_size = alf_client.get_track_size( track_id );
    vector<FrameInfo> track_db_iterator = alf_client.get_frames( track_id, 0, track_size );

    // Create a video player and a video display
    FramePlayer player( alf_client.get_video_width(), alf_client.get_video_height() );

    for ( FrameInfo fi : track_db_iterator ) {
      // Get the raw frame data from alfalfa video directory
      const string chunk = web.get_chunk( fi );
      // Decode the frame
      Optional<RasterHandle> raster_optional = player.decode( chunk );
      // If the raster is decoded successfully, dump the raster to stdout
      if( raster_optional.initialized() ) {
        raster_optional.get().get().dump( stdout ); //First get() for optional, second for RasterHandle
      }
    }
  } catch ( const exception & e ) {
      print_exception( argv[ 0 ], e );
      return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
