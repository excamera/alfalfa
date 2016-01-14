#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/security/server_credentials.h>

#include "player.hh"
#include "exception.hh"
#include "alfalfa_video_service.hh"
#include "alfalfa_video_client.hh"
#include "manifests.hh"
#include "frame_db.hh"
#include "frame_db.hh"

using namespace std;
using namespace grpc;

void start_video_server( unique_ptr<Server> & server )
{
  server->Wait();
}

int main( int argc, char *argv[] )
{
  try {
    if ( argc != 4 ) {
      cerr << "Usage: " << argv[ 0 ] << " <alf-file> <server-address> <track-id>" << endl;
      return EXIT_FAILURE;
    }

    string alf_path( argv[ 1 ] );
    string server_address( argv[ 2 ] );

    AlfalfaVideoServiceImpl service( alf_path );
    ServerBuilder builder;
    builder.AddListeningPort( server_address, grpc::InsecureServerCredentials() );
    builder.RegisterService( &service );
    unique_ptr<Server> server( builder.BuildAndStart() );

    thread server_thread( start_video_server, ref( server ) );
    std::this_thread::sleep_for( std::chrono::seconds( 1 ) );

    AlfalfaVideoClient alf_client( server_address );

    // Use string stream to convert track_id string to a size_t
    stringstream type_converter( argv[ 3 ] );
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
      Chunk frame = Chunk( alf_client.get_chunk( fi ) );
      // Decode the frame
      Optional<RasterHandle> raster_optional = player.decode(frame);
      // If the raster is decoded successfully, dump the raster to stdout
      if( raster_optional.initialized() ) {
        raster_optional.get().get().dump( stdout ); //First get() for optional, second for RasterHandle
      }
    }

    server_thread.detach();
  } catch ( const exception & e ) {
      print_exception( argv[ 0 ], e );
      return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
