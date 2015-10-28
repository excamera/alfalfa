#ifndef FILESYSTEM_HH
#define FILESYSTEM_HH

#include <string>

class FileSystem
{
public:
  static bool exists( const std::string & path );
  static bool is_directory( const std::string & path );
  static bool is_regular_file( const std::string & path );
  static bool create_directory( const std::string & path );
  static bool is_directory_empty( const std::string & directory );
  static bool create_symbolic_link( const std::string & path1, const std::string & path2 );
  static bool change_directory( const std::string & path );

  static std::string append( const std::string & path1, const std::string & path2 );
  static std::string get_basename( const std::string & path );
  static std::string get_realpath( const std::string & path );
};

#endif /* FILESYSTEM_HH */
