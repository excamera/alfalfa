#include <string>

#include "alfalfa_video.hh"

using namespace std;

AlfalfaVideo::VideoDirectory::VideoDirectory( const string & )
{
  /* XXX need to create directory if create is true,
     and then try to open it (and throw exception if failure) */
}

AlfalfaVideo::AlfalfaVideo( const string & directory_name )
  : directory_( directory_name ),
    video_files_(), /* XXX */
    raster_list_( directory_.raster_list_filename() ),
    quality_db_( directory_.quality_db_filename() ),
    frame_db_( directory_.frame_db_filename() ),
    trajectory_db_( directory_.trajectory_db_filename() )
{}


