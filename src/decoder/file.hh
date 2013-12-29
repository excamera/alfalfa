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
  Chunk chunk_;

public:
  File( const std::string & filename );
  ~File();

  const Chunk & chunk( void ) const { return chunk_; }
  const Chunk operator() ( const uint64_t & offset, const uint64_t & length ) const
  {
    return chunk_( offset, length );
  }
};

#endif /* FILE_HH */
