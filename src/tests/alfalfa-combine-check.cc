#include "sysexits.h"

#include "alfalfa_video.hh"
#include "filesystem.hh"
#include "tracking_player.hh"

using namespace std;

/* Combine test checker. Ensures that the combined video has the right entries
   in the raster list, quality db and frame db. */
void alfalfa_combine_test_check( string alfalfa_video_dir, string alfalfa_video_combined_dir ) {
  PlayableAlfalfaVideo alfalfa_video( alfalfa_video_dir );
  PlayableAlfalfaVideo alfalfa_video_combined( alfalfa_video_combined_dir );

  // First, make sure that the raster lists are identical
  if ( !alfalfa_video.equal_raster_lists( alfalfa_video_combined ) ) {
    throw invalid_argument( "raster lists don't match!" );
  }

  // Now, check the quality db
  for ( auto quality_data = alfalfa_video.get_quality_data();
        quality_data.first != quality_data.second; quality_data.first++ ) {
    bool found;
    found = false;
    auto original_results = alfalfa_video_combined.get_quality_data_by_original_raster( quality_data.first->original_raster );
    while ( original_results.first != original_results.second ) {
      if ( original_results.first->approximate_raster == quality_data.first->approximate_raster and
           original_results.first->quality == quality_data.first->quality )
        found = true;
      original_results.first++;
    }
    if ( not found )
      throw logic_error( "quality_data not found in combined video." );

    found = false;
    auto approx_results = alfalfa_video_combined.get_quality_data_by_approximate_raster( quality_data.first->approximate_raster );
    while ( approx_results.first != approx_results.second ) {
      if ( approx_results.first->original_raster == quality_data.first->original_raster and
           approx_results.first->quality == quality_data.first->quality )
        found = true;
      approx_results.first++;
    }
    if ( not found )
      throw logic_error( "quality_data not found in combined video. (2)" );
  }

  // Now check the frame db
  auto frames = alfalfa_video.get_frames();
  for ( auto it = frames.first; it != frames.second; it++ ) {
    if ( not alfalfa_video_combined.has_frame_name( it->name() ) )
      throw logic_error( "frame missing" );
  }

  // Now check the track db
  auto track_ids = alfalfa_video.get_track_ids();
  for ( ; track_ids.first != track_ids.second; track_ids.first++ ) {
    bool found_matching_track = false;
    auto track_ids_combined = alfalfa_video_combined.get_track_ids();
    for ( ; track_ids_combined.first != track_ids_combined.second; track_ids_combined.first++ ) {
      auto track_frames = alfalfa_video.get_frames( *track_ids.first );
      auto track_frames_combined = alfalfa_video_combined.get_frames( *track_ids_combined.first );
      bool found = true;
      while ( track_frames.first != track_frames.second ||
              track_frames_combined.first != track_frames_combined.second ) {
        if ( not ( ( track_frames.first->source_hash() ==
                     track_frames_combined.first->source_hash() ) &&
                   ( track_frames.first->target_hash() ==
                     track_frames_combined.first->target_hash() ) ) ) {
          found = false;
          break;
        }
        track_frames.first++;
        track_frames_combined.first++;
      }
      if ( not ( ( track_frames.first == track_frames.second ) and
                 ( track_frames_combined.first == track_frames_combined.second ) ) )
        found = false;
      if ( found )
        found_matching_track = true;
    }
    if ( not found_matching_track )
      throw logic_error( "could not find track in combined video's track db" );
  }
}

int main(int argc, char const *argv[]) {
  try {
    if ( argc != 4 ) {
      cerr << "usage: combine-test <alfalfa-video-dir1> <alfalfa-video-dir2> <alfalfa-video-dir-combined>" << endl;
      return EX_USAGE;
    }

    string alfalfa_video_dir1( argv[ 1 ] );
    string alfalfa_video_dir2( argv[ 2 ] );
    string alfalfa_video_dir_combined( argv[ 3 ] );

    alfalfa_combine_test_check( alfalfa_video_dir1, alfalfa_video_dir_combined );
    alfalfa_combine_test_check( alfalfa_video_dir2, alfalfa_video_dir_combined );
  } catch (const exception &e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
