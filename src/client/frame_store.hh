#ifndef FRAME_STORE_HH
#define FRAME_STORE_HH

#include <string>
#include <cstdint>
#include <unordered_set>
#include <unordered_map>

#include "file_descriptor.hh"
#include "chunk.hh"
#include "temp_file.hh"

class BackingStore
{
private:
  TempFile file_;
  uint64_t file_size_;

public:
  BackingStore();
  uint64_t append( const Chunk & chunk );
  std::string read( const uint64_t offset, const size_t length ) const;
};

class FrameStore
{
private:
  std::unordered_map<uint64_t, std::pair<uint64_t, size_t>> local_frame_db_ {};
  /* mapping from global offset => local offset and length */
  BackingStore backing_store_ {};
  std::unordered_set<uint64_t> pending_ {}; /* field is the offset */
  
public:
  void insert_frame( const uint64_t global_offset,
		     const Chunk & chunk );
  
  bool has_frame( const uint64_t global_offset ) const;

  std::string coded_frame( const uint64_t global_offset ) const;

  void mark_frame_pending( const uint64_t global_offset );
  bool is_frame_pending( const uint64_t global_offset ) const;
  bool is_anything_pending() const;
};

#endif /* FRAME_STORE_HH */
