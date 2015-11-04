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

bool FileSystem::exists( const std::string & path )
{
  struct stat buffer;
  return stat( path.c_str(), &buffer ) == 0;
}

bool FileSystem::is_directory( const std::string & path )
{
  struct stat buffer;
  SystemCall( "stat", stat( path.c_str(), &buffer ) );

  return S_ISDIR( buffer.st_mode );
}

bool FileSystem::is_regular_file( const std::string & path )
{
  struct stat buffer;
  SystemCall( "stat", stat( path.c_str(), &buffer ) );

  return S_ISREG( buffer.st_mode );
}

void FileSystem::create_directory( const std::string & path )
{
  SystemCall( "mkdir",
	      mkdir( path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH ) );
}

std::string FileSystem::append( const std::string & path1, const std::string & path2 )
{
  if ( path1.length() == 0 or path2.length() == 0 ) {
    return path1 + path2;
  }

  if ( path1[ path1.length() - 1] == '/' and path2[0] == '/' ) {
    return path1 + path2.substr(1);
  }
  else if ( path1[ path1.length() - 1] == '/' and path2[0] != '/' ) {
    return path1 + path2;
  }
  else if ( path1[ path1.length() - 1] != '/' and path2[0] == '/' ) {
    return path1 + path2;
  }
  else {
    return path1 + "/" + path2;
  }
}

bool FileSystem::is_directory_empty( const std::string & directory )
{
  struct Closedir {
    void operator()( DIR *x ) const { SystemCall( "closedir", closedir( x ) ); }
  };

  unique_ptr<DIR, Closedir> dp { opendir( directory.c_str() ) };

  if ( not dp ) {
    throw unix_error( "opendir (" + directory + ")" );
  }

  while ( const dirent *ent = readdir( dp.get() ) ) {
    const string entry_name { ent->d_name };
    if ( entry_name != "." and entry_name != ".." ) {
      return false;
    }
  }

  return true;
}

void FileSystem::create_symbolic_link( const std::string & path1, const std::string & path2 )
{
  SystemCall( "symlink", symlink( path1.c_str(), path2.c_str() ) );
}

void FileSystem::change_directory( const std::string & path )
{
  SystemCall( "chdir", chdir( path.c_str() ) );
}

std::string FileSystem::get_basename( const std::string & path )
{
  return basename( path.c_str() );
}

std::string FileSystem::get_realpath( const std::string & path )
{
  char resolved_path[ PATH_MAX ];

  if ( realpath( path.c_str(), resolved_path ) == nullptr ) {
    throw unix_error( "realpath + (" + path + ")" );
  }
  
  return resolved_path;
}
