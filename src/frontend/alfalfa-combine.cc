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
      cerr << "usage: alfalfa-combine <alf-1> <alf-2> <destination-dir>" << endl;
      return EX_USAGE;
    }

    string alf1_path( argv[ 1 ] );
    string alf2_path( argv[ 2 ] );
    string destination_dir( argv[ 3 ] );

    FileSystem::create_directory( destination_dir );

    AlfalfaVideo alf1( alf1_path, OpenMode::READ );
    AlfalfaVideo alf2( alf2_path, OpenMode::READ );
    AlfalfaVideo res_video( destination_dir, OpenMode::TRUNCATE );

    res_video.combine( alf1 );
    res_video.combine( alf2 );

    res_video.save();
  } catch (const exception &e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
