#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>

#include <iostream>
#include <fstream>
#include <string>

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
  try
  {
    if ( argc != 4 ) {
      cerr << "Usage: " << argv[ 0 ] << " <alf-1> <alf-2> <destination-dir>" << endl;
      return EX_USAGE;
    }

    string alf1_path( argv[ 1 ] );
    string alf2_path( argv[ 2 ] );
    string destination_dir( argv[ 3 ] );

    FileSystem::create_directory( destination_dir );

    PlayableAlfalfaVideo alf1( alf1_path );
    PlayableAlfalfaVideo alf2( alf2_path );
    WritableAlfalfaVideo res_video( destination_dir, alf1.get_info().fourcc, alf1.get_info().width, alf1.get_info().height );

    combine ( res_video, alf1 );
    combine ( res_video, alf2 );

    res_video.save();
  } catch (const exception &e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
