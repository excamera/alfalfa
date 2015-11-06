#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>
#include <iomanip>

#include "exception.hh"
#include "alfalfa_video.hh"
#include "manifests.hh"
#include "frame_db.hh"
#include "filesystem.hh"
#include "frame_db.hh"

using namespace std;

/*
  Alfalfa Describe lists all the tracks in an alfalfa video
  providing the average and minimum quality scores and the total
  coded size of the track. Also, Alfalfa Describe gives summary
  information like the number of frames in the video and the width
  and height of the video.
*/

int main( int argc, char const *argv[] )
{
  //Describes an alfalfa video from an alfalfa directory
  if ( argc != 2 ) {
    cerr << "usage: alfalfa-describe <alf>" << endl;
    return EX_USAGE;
  }

  //Get path to alfalfa video and form alfalfa object
  string alf_path{ FileSystem::get_realpath( argv[ 1 ] ) };
  AlfalfaVideo alf( alf_path, OpenMode::READ );

  //Throw expection if the directory does not contain a valid alfalfa video
  if ( not alf.good() ) {
    cerr << "'" << alf_path << "' is not a valid Alfalfa video." << endl;
    return EX_DATAERR;
  }

  //Get databases from alfalfa video
  QualityDB quality_db = alf.quality_db();
  TrackDB track_db = alf.track_db();
  FrameDB frame_db = alf.frame_db();

  //Get an iterator to the beginning and end of track 0 (currently cannot iterate through tracks)
  pair<TrackDBCollectionByIds::iterator, TrackDBCollectionByIds::iterator> track_iterator = track_db.search_by_track_id( 0 );
  TrackDBCollectionByIds::iterator beginning = track_iterator.first;
  TrackDBCollectionByIds::iterator end = track_iterator.second;

  //Initialize variables
  uint32_t frame_count = 0;
  double quality_sum = 0.0;
  double minimum_quality = 1.0;
  uint64_t total_coded_size = 0;

  //Set the track id
  size_t track_id = (*beginning).track_id;

  //Loop through one track until the iterator reaches the end
  while(beginning != end)
  {
    TrackData td = *beginning;
    //We only count shown frames for quality and frame count
    if(td.target_hash.shown) {
      //Get an iterator to the quality data for a frame and read quality member variable
      pair<QualityDBCollectionByApproximateRaster::iterator, QualityDBCollectionByApproximateRaster::iterator> quality_iterator = quality_db.search_by_approximate_raster( td.target_hash.output_hash );
      double quality = (*quality_iterator.first).quality;
      quality_sum += quality;
      //Set the minimum quality if it is less than the current minimum
      if(quality < minimum_quality) minimum_quality = quality;
      frame_count++;
    }
    //Get an iterator to the frame even if it is not shown. Total coded size includes both shown and unshown frames.
    pair<FrameDataSetCollectionByOutputHash::iterator, FrameDataSetCollectionByOutputHash::iterator> frame_iterator = frame_db.search_by_output_hash( td.target_hash.output_hash );
    uint64_t frame_length = (*frame_iterator.first).length();
    total_coded_size += frame_length;
    //Increment iterator to get next frame in track
    beginning++;
  }
  //Calculate average quality for track and print out relevant statistics
  double average_quality = quality_sum / frame_count;
  cout << "Track ID: " << track_id << endl;
  cout << "\t Total Coded Size: " << total_coded_size << " bytes" << endl;
  cout << "\t Minimum Quality: " << minimum_quality << endl;
  cout << "\t Average Quality: " << average_quality << endl;
  cout << "Frame Count: " << frame_count << endl;
  cout << "Video Width: " << alf.video_manifest().width() << endl;
  cout << "Video Height: " << alf.video_manifest().height() << endl;

  return 0;
}
