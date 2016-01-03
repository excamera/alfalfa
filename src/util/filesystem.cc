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

void FileSystem::create_directory( const string & path )
{
  SystemCall( "mkdir " + path,
	      mkdir( path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH ) );
}
