#include <iostream>
#include <sstream>

#include "player.hh"
#include "display.hh"
#include "exception.hh"
#include "alfalfa_video.hh"
#include "manifests.hh"
#include "frame_db.hh"
#include "filesystem.hh"
#include "frame_db.hh"

using namespace std;

int main( int argc, char *argv[] )
{
  try {
    if ( argc != 3 ) {
      cerr << "Usage: " << argv[ 0 ] << " <alf> <track_id>" << endl;
      return EXIT_FAILURE;
    }

    string alf_path( argv[ 1 ] );
    PlayableAlfalfaVideo alf( alf_path );

    stringstream type_converter( argv[2] );
    size_t track_id;
    type_converter >> track_id;

    pair<TrackDBIterator, TrackDBIterator> track_db_iterator = alf.get_frames( track_id );
    TrackDBIterator track_beginning = track_db_iterator.first;
    TrackDBIterator track_end = track_db_iterator.second;

    FramePlayer player( alf.video_manifest().width(), alf.video_manifest().height() );
    VideoDisplay display { player.example_raster() };

    while(track_beginning != track_end) {
      FrameInfo fi = *track_beginning;
      Chunk frame = alf.get_chunk( fi );
      Optional<RasterHandle> raster_optional = player.decode(frame);
      if( raster_optional.initialized() ) {
        display.draw( raster_optional.get() );
      }
      track_beginning++;
    }
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
