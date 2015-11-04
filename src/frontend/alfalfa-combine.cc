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

using namespace std;

int main( int argc, char const *argv[] )
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
  res_video.combine( alf1 );
  res_video.combine( alf2 );

  // creating symbolic links
  for ( auto const & ivf_filename : alf1.frame_db().ivf_files() ) {
    std::string ivf_filepath{ FileSystem::get_realpath( FileSystem::append( alf1_path, ivf_filename ) ) };
    std::string ivf_symlink_path{ FileSystem::append( destination_dir, ivf_filename ) };

    FileSystem::create_symbolic_link( ivf_filepath,  ivf_symlink_path );
  }

  for ( auto const & ivf_filename : alf2.frame_db().ivf_files() ) {
    std::string ivf_filepath{ FileSystem::get_realpath( FileSystem::append( alf2_path, ivf_filename ) ) };
    std::string ivf_symlink_path{ FileSystem::append( destination_dir, ivf_filename ) };

    FileSystem::create_symbolic_link( ivf_filepath,  ivf_symlink_path );
  }

  res_video.save();

  return 0;
}
