#include <string>

#include "exception.hh"
#include "alfalfa_video.hh"
#include "tracking_player.hh"
#include "filesystem.hh"

AlfalfaVideo::VideoDirectory::VideoDirectory( const std::string & path )
  : directory_path_( path )
{
}

std::string AlfalfaVideo::VideoDirectory::video_manifest_filename() const
{
  return FileSystem::append( directory_path_, VIDEO_MANIFEST_FILENAME );
}

std::string AlfalfaVideo::VideoDirectory::raster_list_filename() const
{
  return FileSystem::append( directory_path_, RASTER_LIST_FILENAME );
}

std::string AlfalfaVideo::VideoDirectory::quality_db_filename() const
{
  return FileSystem::append( directory_path_, QUALITY_DB_FILENAME );
}

std::string AlfalfaVideo::VideoDirectory::frame_db_filename() const
{
  return FileSystem::append( directory_path_, FRAME_DB_FILENAME );
}

std::string AlfalfaVideo::VideoDirectory::track_db_filename() const
{
  return FileSystem::append( directory_path_, TRACK_DB_FILENAME );
}

AlfalfaVideo::AlfalfaVideo( const string & directory_name, OpenMode mode )
  : directory_( FileSystem::get_realpath( directory_name ) ), mode_( mode ),
    video_manifest_( directory_.video_manifest_filename(), "ALFAVDMF", mode ),
    raster_list_( directory_.raster_list_filename(), "ALFARSLS", mode ),
    quality_db_( directory_.quality_db_filename(), "ALFAQLDB", mode ),
    frame_db_( directory_.frame_db_filename(), "ALFAFRDB", mode ),
    track_db_( directory_.track_db_filename(), "ALFATRDB", mode )
{}

bool AlfalfaVideo::good() const
{
  return video_manifest_.good() and raster_list_.good() and quality_db_.good()
    and frame_db_.good() and track_db_.good();
}

bool AlfalfaVideo::can_combine( const AlfalfaVideo & video )
{
  return (
    raster_list_.size() == 0 or
    (
      video_manifest_.width() == video.video_manifest_.width() and
      video_manifest_.height() == video.video_manifest_.height() and
      raster_list_ == video.raster_list_
    )
  );
}

void AlfalfaVideo::combine( const AlfalfaVideo & video )
{
if ( not can_combine( video ) ) {
    throw Invalid( "cannot combine: raster lists are not the same." );
  }
  else if ( raster_list_.size() == 0 ) {
    raster_list_.merge( video.raster_list_ );
  }
  video_manifest_.set_info( video.video_manifest_.info() );
  quality_db_.merge( video.quality_db_ );
  frame_db_.merge( video.frame_db_ );
  track_db_.merge( video.track_db_ );
}

void AlfalfaVideo::import_ivf_file( const string & filename )
{
  TrackingPlayer player( FileSystem::append( directory_.path(), filename ) );
  IVF ivf( FileSystem::append( directory_.path(), filename ) );
  VideoInfo info( ivf.fourcc(), ivf.width(), ivf.height(), ivf.frame_rate(),
    ivf.time_scale(), ivf.frame_count() );

  video_manifest_.set_info( info );

  size_t frame_id = 1;
  while ( not player.eof() ) {
    FrameInfo next_frame( player.serialize_next().first );

    raster_list_.insert(
      RasterData{
        next_frame.target_hash().output_hash
      }
    );

    // When importing an alfalfa video, all approximate rasters have a quality of 1.0
    quality_db_.insert(
      QualityData{
        next_frame.target_hash().output_hash,
        next_frame.target_hash().output_hash,
        1.0
      }
    );

    frame_db_.insert( next_frame );

    track_db_.insert(
      TrackData{
        0,
        frame_id,
        next_frame.source_hash(),
        next_frame.target_hash()
      }
    );
    frame_id++;
  }

  save();
}

bool AlfalfaVideo::save()
{
  return video_manifest_.serialize() and
    raster_list_.serialize() and
    quality_db_.serialize() and
    track_db_.serialize() and
    frame_db_.serialize();
}
