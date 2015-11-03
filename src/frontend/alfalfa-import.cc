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
  if ( argc != 3 ) {
    cerr << "usage: alfalfa-import <ivf-file> <destination-dir>" << endl;
    return EX_USAGE;
  }

  string ivf_file_path( argv[ 1 ] );
  string destination_dir( argv[ 2 ] );

  if ( not FileSystem::exists( ivf_file_path ) ) {
    cerr << "ivf file not found." << endl;
    return EX_NOINPUT;
  }

  if ( FileSystem::is_directory( destination_dir ) ) {
    if ( not FileSystem::is_directory_empty( destination_dir ) ) {
      cerr << "destination directory is not empty." << endl;
      return EX_CANTCREAT;
    }
  }
  else {
    if ( not FileSystem::create_directory( destination_dir ) ) {
      cerr << "cannot create destination directory." << endl;
      return EX_CANTCREAT;
    }
  }

  std::string ivf_symlink_path = FileSystem::append( destination_dir,
    FileSystem::get_basename( ivf_file_path ) );

  if ( not FileSystem::create_symbolic_link( FileSystem::get_realpath( ivf_file_path ),
      ivf_symlink_path ) ) {
    cerr << "cannot create symlink for ivf file." << endl;
    return EX_CANTCREAT;
  }

  if( not FileSystem::change_directory( destination_dir ) ) {
    cerr << "cannot change directory." << endl;
    return EX_CANTCREAT;
  }

  AlfalfaVideo alfalfa_video( ".", OpenMode::TRUNCATE );
  alfalfa_video.import_ivf_file( FileSystem::get_basename( ivf_file_path ) );

  return 0;
}
