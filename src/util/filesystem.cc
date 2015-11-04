#include "filesystem.hh"
#include "exception.hh"

#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <memory>

using namespace std;

void FileSystem::create_directory( const std::string & path )
{
  SystemCall( "mkdir " + path,
	      mkdir( path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH ) );
}

std::string FileSystem::append( const std::string & path1, const std::string & path2 )
{
  return path1 + "/" + path2;
}

void FileSystem::change_directory( const std::string & path )
{
  SystemCall( "chdir " + path, chdir( path.c_str() ) );
}

std::string FileSystem::get_basename( const std::string & path )
{
  return basename( path.c_str() );
}

std::string FileSystem::get_realpath( const std::string & path )
{
  char resolved_path[ PATH_MAX ];

  if ( realpath( path.c_str(), resolved_path ) == nullptr ) {
    throw unix_error( "realpath (" + path + ")" );
  }
  
  return resolved_path;
}

void FileSystem::create_symbolic_link( const std::string & path1, const std::string & path2 )
{
  SystemCall( "symlink", symlink( path1.c_str(), path2.c_str() ) );
}
