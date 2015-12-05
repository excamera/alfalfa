#include <iostream>
#include <sstream>

#include "player.hh"
#include "display.hh"
#include "exception.hh"
#include "alfalfa_video.hh"
#include "manifests.hh"
#include "frame_db.hh"
#include "filesystem.hh"

using namespace std;

//Plays one track of an alfalfa video
int main( int argc, char *argv[] )
{
  try {
    if ( argc != 3 ) {
      cerr << "Usage: " << argv[ 0 ] << " <alf> <track_id>" << endl;
      return EXIT_FAILURE;
    }

    //Create the path for the alfalfa video directory and call the constructor
    string alf_path( argv[ 1 ] );
    PlayableAlfalfaVideo alf( alf_path );

    //Use string stream to convert track_id string to a size_t
    stringstream type_converter( argv[2] );
    size_t track_id;
    type_converter >> track_id;

    //Get the track iterators from the alfalfa video for the specified track
    //A track iterator can be used to find the frame info objects for a track
    pair<TrackDBIterator, TrackDBIterator> track_db_iterator = alf.get_frames( track_id );
    TrackDBIterator track_beginning = track_db_iterator.first;
    TrackDBIterator track_end = track_db_iterator.second;

    //Create a video player and a video display
    FramePlayer player( alf.get_info().width, alf.get_info().height );
    VideoDisplay display { player.example_raster() };

    //Loop over the entire track frame by frame
    while(track_beginning != track_end) {
      FrameInfo fi = *track_beginning;

      //Get the raw frame data from alfalfa video directory
      Chunk frame = alf.get_chunk( fi );

      //Decode the frame
      Optional<RasterHandle> raster_optional = player.decode(frame);

      //If the raster is decoded successfully, draw the raster to the display
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
