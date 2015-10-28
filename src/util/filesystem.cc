#include "filesystem.hh"

#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <cstdlib>

using namespace std;

bool FileSystem::exists( const std::string & path )
{
  struct stat buffer;
  return stat( path.c_str(), &buffer ) == 0;
}

bool FileSystem::is_directory( const std::string & path )
{
  struct stat buffer;
  if ( stat( path.c_str(), &buffer ) != 0) {
    return false;
  }
  return S_ISDIR( buffer.st_mode );
}

bool FileSystem::is_regular_file( const std::string & path )
{
  struct stat buffer;
  if ( stat( path.c_str(), &buffer ) != 0 ) {
    return false;
  }
  return S_ISREG( buffer.st_mode );
}

bool FileSystem::create_directory( const std::string & path )
{
  if ( is_directory( path ) ) {
    return false;
  }

  return mkdir( path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH ) == 0;
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
  dirent * ent;
  int count = 0;

  DIR * dirfd = opendir( directory.c_str() );

  if ( not dirfd ) {
    return false;
  }

  while ( count <= 2 and ( ent = readdir( dirfd ) ) ) {
    count++;
  }

  closedir( dirfd );
  return ( count <= 2 );
}

bool FileSystem::create_symbolic_link( const std::string & path1, const std::string & path2 )
{
  return symlink( path1.c_str(), path2.c_str() ) == 0;
}

bool FileSystem::change_directory( const std::string & path )
{
  return chdir( path.c_str() ) == 0;
}

std::string FileSystem::get_basename( const std::string & path )
{
  size_t last_slash = path.rfind( '/' );

  if ( last_slash == string::npos ) {
    return path;
  }

  return path.substr( last_slash + 1 );
}

std::string FileSystem::get_realpath( const std::string & path )
{
  char resolved_path[ PATH_MAX ];
  if ( realpath( path.c_str(), resolved_path ) == NULL ) {
    return path;
  }
  return resolved_path;
}
