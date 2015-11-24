#include <iostream>
#include <sstream>
#include <set>
#include <algorithm>

#include "player.hh"
#include "display.hh"
#include "exception.hh"
#include "alfalfa_video.hh"
#include "manifests.hh"
#include "frame_db.hh"
#include "filesystem.hh"

using namespace std;

/*
  Alfalfa Verify decodes all tracks for an alfalfa video in order
  and verifies that the decoder hash matches the target hash for
  each frame. At the end, it prints out a list of frames that are
  not included in any track.
*/

int main( int argc, char *argv[] )
{

  int return_code = EXIT_SUCCESS;

  try {
    if ( argc != 2 ) {
      cerr << "Usage: " << argv[ 0 ] << " <alf> " << endl;
      return EXIT_FAILURE;
    }

    //Create the path for the alfalfa video directory and call the constructor
    string alf_path( argv[ 1 ] );
    PlayableAlfalfaVideo alf( alf_path );

    //Get the track database and the frame database
    TrackDB track_db = alf.track_db();
    FrameDB frame_db = alf.frame_db();

    //Get all the track ids for a video
    auto track_ids_iterator = alf.get_track_ids();

    //Create a set to keep track of frames in the track db
    set<string> frames_in_track_db = *new set<string>();

    for ( auto it = track_ids_iterator.first; it != track_ids_iterator.second; it++ )
    {
      size_t track_id = *it;

      //Get the track iterators to iterate over the frames of a track
      pair<TrackDBIterator, TrackDBIterator> track_db_iterator = alf.get_frames( track_id );
      TrackDBIterator track_beginning = track_db_iterator.first;
      TrackDBIterator track_end = track_db_iterator.second;

      //Get the frame player, which contains the decoder
      FramePlayer player( alf.video_manifest().width(), alf.video_manifest().height() );

      //Loop over the entire track frame by frame
      while( track_beginning != track_end ) {
        FrameInfo fi = *track_beginning;

        //Add frame name to frames set to check for unused frames
        frames_in_track_db.insert( fi.frame_name() );

        //Get the raw frame data from alfalfa video directory
        Chunk frame = alf.get_chunk( fi );

        //Decode the frame
        Optional<RasterHandle> raster_optional = player.decode(frame);

        //If the raster is decoded successfully, check to make sure the DecoderHash matches the frame name
        if( raster_optional.initialized() ) {
          if( raster_optional.get().hash() != fi.target_hash().output_hash ) {
            cerr << "ERROR: Could not verify track with id: " << track_id << endl;
            cerr << "Decoded hash " << raster_optional.get().hash() << " does not match expected target hash " << fi.target_hash().output_hash << endl;
            return_code = EXIT_FAILURE;
          }
        }

        track_beginning++;
      }
    }

    //Get iterators to the frame db
    auto frame_db_iterator = alf.get_frames();
    auto frames_beginning = frame_db_iterator.first;
    auto frames_end = frame_db_iterator.second;

    //Iterate through the frame db and add all the frame names to a set
    set<string> frames_in_frame_db = *new set<string>();
    while( frames_beginning != frames_end ) {
      FrameInfo fi = *frames_beginning;
      frames_in_frame_db.insert( fi.frame_name() );
      frames_beginning++;
    }

    //Take the set difference between frames in the frame db and frames in the track db to find unused frames
    set<string> unused_frames;
    set_difference( frames_in_frame_db.begin(), frames_in_frame_db.end(), frames_in_track_db.begin(), frames_in_track_db.end(), inserter(unused_frames, unused_frames.end() ) );

    if( unused_frames.size() > 0 ) {
      cerr << "WARNING: This alfalfa video contains unused frames." << endl;
      cerr << "\tUnused frames are as follows: " << endl;
      return_code = EXIT_FAILURE;
    }

    for( auto frame_name : unused_frames ) {
      cerr << "\t" << frame_name << endl;
    }

  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  if( return_code == EXIT_SUCCESS ) {
    cout << "SUCCESS: alfalfa-verify has successfuly verfied this alfalfa video!" << endl;
  }
  
  return return_code;
}
