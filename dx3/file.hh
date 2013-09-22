#ifndef FILE_HH
#define FILE_HH

/* memory-mapped read-only file wrapper */

#include <string>

#include "file_descriptor.hh"

class File
{
private:
  FileDescriptor fd_;
  uint64_t size_;
  uint8_t *buffer_;

public:
  File( const std::string & filename );
  ~File();

  uint64_t size( void ) const { return size_; }
  const uint8_t & operator[] ( uint64_t index ) const { return buffer_[ index ]; }

  /* disallow assigning or copying */
  File( const File & other ) = delete;
  const File & operator=( const File & other ) = delete;
};

#endif /* FILE_HH */
