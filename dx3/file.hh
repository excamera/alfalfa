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
  Block block_;

public:
  File( const std::string & filename );
  ~File();

  const Block & block( void ) const { return block_; }
  const Block operator() ( const uint64_t & offset, const uint64_t & length ) const
  {
    return block_( offset, length );
  }
};

#endif /* FILE_HH */
