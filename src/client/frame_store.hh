#ifndef FRAME_STORE_HH
#define FRAME_STORE_HH

#include <string>
#include <cstdint>
#include <unordered_set>

#include "file_descriptor.hh"
#include "frame_db.hh"
#include "chunk.hh"
#include "temp_file.hh"

class BackingStore
{
private:
  TempFile file_;
  uint64_t file_size_;

public:
  BackingStore();
  size_t append( const Chunk & chunk );
  std::string read( const size_t offset, const size_t length ) const;
};

class FrameStore
{
private:
  FrameDB local_frame_db_ {};
  BackingStore backing_store_ {};
  std::unordered_set<FrameName, boost::hash<FrameName>> pending_ {};
  
public:
  void insert_frame( FrameInfo frame_info,
		     const Chunk & chunk );
  
  bool has_frame( const FrameName & frame_name ) const;

  std::string coded_frame( const FrameName & frame_name ) const;

  void mark_frame_pending( const FrameName & name );
  bool is_frame_pending( const FrameName & name ) const;
  bool is_anything_pending() const;
};

#endif /* FRAME_STORE_HH */
