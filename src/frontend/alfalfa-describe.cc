#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>
#include <iomanip>

#include "exception.hh"
#include "alfalfa_video.hh"
#include "manifests.hh"
#include "frame_db.hh"
#include "filesystem.hh"

using namespace std;

/*
  Alfalfa Describe lists all the tracks in an alfalfa video
  providing the average and minimum quality scores, the total
  coded size of the track, and the bitrate of the track. Also,
  Alfalfa Describe gives summary information like the number of
  frames in the video, the width and height of the video, and
  the duration of the video in seconds.
*/

int main( int argc, char const *argv[] )
{
  try
  {
    // Describes an alfalfa video from an alfalfa directory
    if ( argc != 2 ) {
      cerr << "Usage: " << argv[ 0 ] << " <alf>" << endl;
      return EX_USAGE;
    }

    string alf_path( argv[ 1 ] );
    PlayableAlfalfaVideo alf( alf_path );

    // One frame count per video
    uint32_t frame_count = 0;
    // Get frame rate for video
    // FIXME fix the frame ratemake
    double frame_rate = 24.0;
    // Initialize duration
    double duration_in_seconds = 0.0;

    auto track_ids_iterator = alf.get_track_ids();

    // Loop through each track_id
    for ( auto it = track_ids_iterator.first; it != track_ids_iterator.second; it++ )
    {
      size_t track_id = *it;

      // Initialize variables
      duration_in_seconds = 0.0;
      frame_count = 0;
      double quality_sum = 0.0;
      double minimum_quality = 1.0;
      uint64_t total_coded_size = 0;
      // Get an iterator to the beginning and end of a track
      pair<TrackDBIterator, TrackDBIterator> track_db_iterator = alf.get_frames( track_id );
      TrackDBIterator track_beginning = track_db_iterator.first;
      TrackDBIterator track_end = track_db_iterator.second;

      // Loop through one track until the iterator reaches the end
      while ( track_beginning != track_end )
      {
        // Get frame info for frame in track
        FrameInfo fi = *track_beginning;
        if ( fi.target_hash().shown ) {
          // Get an iterator to the quality data for a frame and read quality member variable
          pair<QualityDBCollectionByApproximateRaster::iterator, QualityDBCollectionByApproximateRaster::iterator> quality_iterator = alf.get_quality_data_by_approximate_raster( fi.target_hash().output_hash );
          double quality = ( *quality_iterator.first ).quality;
          quality_sum += quality;

          // Set the minimum quality if it is less than the current minimum
          if ( quality < minimum_quality ) {
            minimum_quality = quality;
          }

          frame_count++;
        }
        // Get an iterator to the frame even if it is not shown. Total coded size includes both shown and unshown frames.
        pair<FrameDataSetCollectionByOutputHash::iterator, FrameDataSetCollectionByOutputHash::iterator> frame_iterator = alf.get_frames_by_output_hash( fi.target_hash().output_hash );

        uint64_t frame_length = ( *frame_iterator.first ).length();

        total_coded_size += frame_length;
        // Increment iterator to get next frame in track
        track_beginning++;
      }
      // Calculate average quality for track and print out relevant statistics
      double average_quality = quality_sum / frame_count;

      // Frame rate is frames per second
      duration_in_seconds = frame_count / frame_rate;

      // Bitrate is bits in the entire video (shown and unshown) over duration
      double track_bitrate = ( total_coded_size * 8 ) / duration_in_seconds;

      cout << "\n\nTrack ID: " << track_id << endl;
      cout << "\t Total Coded Size: " << total_coded_size << " bytes" << endl;
      cout << "\t Bitrate: " << track_bitrate << " bits/sec" << endl;
      cout << "\t Average SSIM Quality: " << average_quality << endl;
      cout << "\t Minimum SSIM Quality: " << minimum_quality << endl;

    }

    cout << "\n\n";
    cout << "Frame Count: " << frame_count << " total frames" << endl;
    cout << "Frame Rate: " << frame_rate << " frames/sec" << endl;
    cout << "Video Duration: " << duration_in_seconds << " sec" << endl;
    cout << "Video Width: " << alf.get_info().width << " pixels" << endl;
    cout << "Video Height: " << alf.get_info().height << " pixels" << endl;
  } catch ( const exception &e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
