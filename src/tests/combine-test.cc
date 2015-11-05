#include "sysexits.h"

#include "alfalfa_video.hh"
#include "filesystem.hh"
#include "tracking_player.hh"

using namespace std;

int alfalfa_combine_test( string ivf_file_path, string alfalfa_video_dir ) {
  int num_found;

  FileSystem::change_directory( alfalfa_video_dir );
  AlfalfaVideo alfalfa_video( ".", OpenMode::READ );

  RasterList & raster_list = alfalfa_video.raster_list();
  QualityDB & quality_db = alfalfa_video.quality_db();
  FrameDB & frame_db = alfalfa_video.frame_db();
  TrackDB & track_db = alfalfa_video.track_db();

  TrackingPlayer player( ivf_file_path );
  size_t frame_id = 1;

  while ( not player.eof() ) {
    FrameInfo next_frame( player.serialize_next().second );

    const size_t raster = next_frame.target_hash().output_hash;
    const size_t approximate_raster = raster;

    // Check raster_list
    if ( not raster_list.has( raster ) )
      return 1;

    // Check quality db
    num_found = 0;
    auto it_orig = 
      quality_db.search_by_original_raster( raster );
    QualityDBCollectionByOriginalRaster::iterator it_begin_orig = it_orig.first;
    QualityDBCollectionByOriginalRaster::iterator it_end_orig = it_orig.second;
    while ( it_begin_orig != it_end_orig ) {
      if ( it_begin_orig->original_raster != raster )
        return 1;
      if (it_begin_orig->approximate_raster == approximate_raster) {
        if (it_begin_orig->quality == 1.0)
          num_found += 1;
      }
      it_begin_orig++;
    }
    if (num_found < 2)
      return 1;

    num_found = 0;
    auto it_approx =
      quality_db.search_by_approximate_raster( approximate_raster );
    QualityDBCollectionByApproximateRaster::iterator it_begin_approx = it_approx.first;
    QualityDBCollectionByApproximateRaster::iterator it_end_approx = it_approx.second;
    while ( it_begin_approx != it_end_approx ) {
      if (it_begin_approx->approximate_raster != approximate_raster )
        return 1;
      if (it_begin_approx->original_raster == raster ) {
        if (it_begin_approx->quality == 1.0)
          num_found += 1;
      }
      it_begin_approx++;
    }
    if (num_found < 2)
      return 1;

    // Check frame db
    num_found = 0;
    auto it_output_hash =
      frame_db.search_by_output_hash( raster );
    FrameDataSetCollectionByOutputHash::iterator it_begin_output_hash = it_output_hash.first;
    FrameDataSetCollectionByOutputHash::iterator it_end_output_hash = it_output_hash.second;
    while ( it_begin_output_hash != it_end_output_hash ) {
      if (it_begin_output_hash->offset() == next_frame.offset() and
          it_begin_output_hash->length() == next_frame.length() and
          it_begin_output_hash->source_hash() == next_frame.source_hash() and
          it_begin_output_hash->target_hash() == next_frame.target_hash())
        num_found += 1;
      it_begin_output_hash++;
    }
    if (num_found < 2)
      return 1;

    // Check track db
    bool found;

    found = false;
    auto it_ids =
      track_db.search_by_track_id( 0 );
    TrackDBCollectionByIds::iterator it_begin_ids = it_ids.first;
    TrackDBCollectionByIds::iterator it_end_ids = it_ids.second;
    while ( it_begin_ids != it_end_ids ) {
      if (it_begin_ids->source_hash == next_frame.source_hash() and
          it_begin_ids->target_hash == next_frame.target_hash() and
          it_begin_ids->frame_id == frame_id)
          found = true;
      it_begin_ids++;
    }
    if (!found) {
      return 1;
    }

    found = false;
    it_ids =
      track_db.search_by_track_id( 1 );
    it_begin_ids = it_ids.first;
    it_end_ids = it_ids.second;
    while ( it_begin_ids != it_end_ids ) {
      if (it_begin_ids->source_hash == next_frame.source_hash() and
          it_begin_ids->target_hash == next_frame.target_hash() and
          it_begin_ids->frame_id == frame_id)
          found = true;
      it_begin_ids++;
    }
    if (!found) {
      return 1;
    }
    
    frame_id++;
  }

  return 0;
}

int main(int argc, char const *argv[]) {
  if ( argc != 3 ) {
    cerr << "usage: combine-test <ivf-file> <alfalfa-video-dir>" << endl;
    return 1;
  }

  string ivf_file_path( argv[ 1 ] );
  string alfalfa_video_dir( argv[ 2 ] );

  return alfalfa_combine_test( ivf_file_path, alfalfa_video_dir );
}
