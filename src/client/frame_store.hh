#ifndef FRAME_STORE_HH
#define FRAME_STORE_HH

#include <string>
#include <cstdint>

#include "file_descriptor.hh"
#include "frame_db.hh"
#include "chunk.hh"

class BackingStore
{
private:
  FileDescriptor fd_;
  uint64_t file_size_;

public:
  BackingStore( const std::string & filename );
  size_t append( const Chunk & chunk );
  std::string read( const size_t offset, const size_t length ) const;
};

class FrameStore
{
private:
  FrameDB local_frame_db_;
  BackingStore backing_store_;

public:
  FrameStore( const std::string & filename );
  
  void insert_frame( FrameInfo frame_info,
		     const Chunk & chunk );
  
  bool has_frame( const FrameName & frame_name ) const;

  std::string coded_frame( const FrameName & frame_name ) const;
};

#endif /* FRAME_STORE_HH */
