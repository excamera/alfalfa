#ifndef FILE_HH
#define FILE_HH

/* memory-mapped read-only file wrapper */

#include <string>

#include "file_descriptor.hh"
#include "block.hh"

class File
{
private:
  FileDescriptor fd_;
  uint64_t size_;
  char *buffer_;

public:
  File( const std::string & filename );
  ~File();

  uint64_t size( void ) const { return size_; }

  const char & operator[] ( const uint64_t & index ) const { return buffer_[ index ]; }

  Block block( const uint64_t & offset, const uint32_t & length );

  /* disallow assigning or copying */
  File( const File & other ) = delete;
  const File & operator=( const File & other ) = delete;

  /* exception */
  class BoundsCheckException {};
};

#endif /* FILE_HH */
