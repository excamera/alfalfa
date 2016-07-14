/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef FILE_HH
#define FILE_HH

/* memory-mapped read-only file wrapper */

#include <string>

#include "file_descriptor.hh"
#include "chunk.hh"
#include "mmap_region.hh"

class File
{
private:
  FileDescriptor fd_;
  size_t size_;
  MMap_Region mmap_region_;
  Chunk chunk_;

public:
  File( const std::string & filename );
  File( FileDescriptor && fd );

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

  size_t size() const { return size_; }
};

#endif /* FILE_HH */
