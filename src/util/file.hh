#ifndef FILE_HH
#define FILE_HH

/* memory-mapped read-only file wrapper */

#include <string>

#include "file_descriptor.hh"
#include "chunk.hh"

class File
{
private:
  FileDescriptor fd_;
  size_t size_;
  uint8_t * buffer_;
  Chunk chunk_;

public:
  File( const std::string & filename );
  ~File();

  const Chunk & chunk( void ) const { return chunk_; }
  const Chunk operator() ( const uint64_t & offset, const uint64_t & length ) const
  {
    return chunk_( offset, length );
  }

  /* Disallow copying */
  File( const File & other ) = delete;
  File & operator=( const File & other ) = delete;

  /* Allow moving */
  File( File && other );
};

#endif /* FILE_HH */
