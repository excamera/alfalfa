#include <string>

#include "exception.hh"
#include "alfalfa_video.hh"
#include "tracking_player.hh"
#include "filesystem.hh"

AlfalfaVideo::VideoDirectory::VideoDirectory( const std::string & path )
: directory_path_( path )
{
  if ( not FileSystem::is_directory( directory_path_ ) ) {
    throw unix_error( "directory does not exist." );
  }
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
  : directory_( directory_name ), mode_( mode ),
    raster_list_( directory_.raster_list_filename(), "ALFARSLS", mode ),
    quality_db_( directory_.quality_db_filename(), "ALFAQLDB", mode ),
    frame_db_( directory_.frame_db_filename(), "ALFAFRDB", mode ),
    track_db_( directory_.track_db_filename(), "ALFATRDB", mode )
{}

bool AlfalfaVideo::good() const
{
  return raster_list_.good() and quality_db_.good() and frame_db_.good() and track_db_.good();
}

void AlfalfaVideo::import_ivf_file( const string & filename )
{
  if ( not FileSystem::exists( filename ) or not FileSystem::is_regular_file( filename ) ) {
    throw unix_error( "ivf file doesn't exist." );
  }

  TrackingPlayer player( filename );

  while ( not player.eof() ) {
    FrameInfo next_frame( player.serialize_next().second );
    next_frame.set_ivf_filename( filename );

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
    // TODO: Need to fix this
    frame_db_.insert( next_frame );
    // TODO: Fix ID here
    track_db_.insert(
      TrackData{
        0,
        next_frame.source_hash(),
        next_frame.target_hash()
      }
    );
  }

  save();
}

bool AlfalfaVideo::save()
{
  return raster_list_.serialize() and
    quality_db_.serialize() and
    track_db_.serialize() and
    frame_db_.serialize();
}
