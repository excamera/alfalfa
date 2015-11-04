#ifndef FILESYSTEM_HH
#define FILESYSTEM_HH

#include <string>

class FileSystem
{
public:
  static void create_directory( const std::string & path );
  static void create_symbolic_link( const std::string & path1, const std::string & path2 );
  static void change_directory( const std::string & path );

  static std::string append( const std::string & path1, const std::string & path2 );
  static std::string get_basename( const std::string & path );
  static std::string get_realpath( const std::string & path );
};

#endif /* FILESYSTEM_HH */
