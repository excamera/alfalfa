#ifndef FILESYSTEM_HH
#define FILESYSTEM_HH

#include <string>

class FileSystem
{
public:
  static void create_directory( const std::string & path );

  static std::string append( const std::string & path1, const std::string & path2 );
};

#endif /* FILESYSTEM_HH */
