#include <cstdlib>
#include <sysexits.h>

#include "exception.hh"
#include "alfalfa_video.hh"
#include "manifests.hh"
#include "frame_db.hh"
#include "tracking_player.hh"
#include "filesystem.hh"
#include "ivf_writer.hh"

using namespace std;

int main( int argc, char const *argv[] )
{
  try {
    if ( argc != 3 ) {
      cerr << "Usage: " << argv[ 0 ] << " <ivf-file> <destination-dir>" << endl;
      return EX_USAGE;
    }

    const string destination_dir( argv[ 2 ] );

    IVF ivf_file( argv[ 1 ] );
  
    FileSystem::create_directory( destination_dir );
    AlfalfaVideo alfalfa_video( destination_dir, OpenMode::TRUNCATE );

    const string new_ivf_filename = "v"; /* short name makes for short framedb entries */

    /* copy each frame into a new file, then close the file */
    {
      IVFWriter imported_ivf_file( FileSystem::append( destination_dir, new_ivf_filename ),
				   ivf_file.fourcc(),
				   ivf_file.width(),
				   ivf_file.height(),
				   ivf_file.frame_rate(),
				   ivf_file.time_scale() );

      /* copy each frame */
      for ( unsigned int i = 0; i < ivf_file.frame_count(); i++ ) {
	imported_ivf_file.append_frame( ivf_file.frame( i ) );
      }
    }
  
    alfalfa_video.import_ivf_file( new_ivf_filename );
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }
    
  return EXIT_SUCCESS;
}
