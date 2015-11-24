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

    const string ivf_file( argv[ 1 ] );
    const IVF ivf( ivf_file );
    const string destination_dir( argv[ 2 ] );

    FileSystem::create_directory( destination_dir );
    WritableAlfalfaVideo alfalfa_video( destination_dir, ivf );

    alfalfa_video.import( ivf_file );
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
