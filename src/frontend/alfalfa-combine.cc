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

    string alf1_path{ FileSystem::get_realpath( argv[ 1 ] ) };
    string alf2_path{ FileSystem::get_realpath( argv[ 2 ] ) };
    string destination_dir{ argv[ 3 ] };

    FileSystem::create_directory( destination_dir );

    destination_dir = FileSystem::get_realpath( destination_dir );

    FileSystem::change_directory( destination_dir );

    AlfalfaVideo alf1( alf1_path, OpenMode::READ );
    AlfalfaVideo alf2( alf2_path, OpenMode::READ );

    if ( not alf1.good() ) {
      cerr << "'" << alf1_path << "' is not a valid Alfalfa video." << endl;
      return EX_DATAERR;
    }

    if ( not alf2.good() ) {
      cerr << "'" << alf2_path << "' is not a valid Alfalfa video." << endl;
      return EX_DATAERR;
    }

    if ( not alf1.can_combine( alf2 ) ) {
      cerr << "cannot combine: raster lists are not the same." << endl;
      return EX_DATAERR;
    }

    AlfalfaVideo res_video( ".", OpenMode::TRUNCATE );
    const string new_ivf_filename = res_video.get_ivf_file_name();

    // TODO: Make sure all the fields below are the same for alf1 and alf2

    /* copy all frames into a single ivf file */
    IVFWriter combined_ivf_writer( FileSystem::append( destination_dir, new_ivf_filename ),
                                   alf1.video_manifest().fourcc(),
                                   alf1.video_manifest().width(),
                                   alf1.video_manifest().height(),
                                   alf1.video_manifest().frame_rate_numerator(),
                                   alf1.video_manifest().frame_rate_denominator() );

    res_video.combine( alf1, combined_ivf_writer );
    res_video.combine( alf2, combined_ivf_writer );

    res_video.save();
  } catch (const exception &e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
