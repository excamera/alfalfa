#ifndef UNCOMPRESSED_CHUNK_HH
#define UNCOMPRESSED_CHUNK_HH

#include "chunk.hh"
#include "loopfilter.hh"

#include <vector>

class UncompressedChunk
{
private:
  enum class ReconstructionFilterType : char {
    Bicubic,
    Bilinear,
    NoFilter
  };

  bool key_frame_;
  ReconstructionFilterType reconstruction_filter_;
  LoopFilterType loop_filter_;
  bool show_frame_;
  bool experimental_;
  bool reference_update_;
  Chunk first_partition_;
  Chunk rest_;

public:
  UncompressedChunk( const Chunk & frame, const uint16_t expected_width, const uint16_t expected_height );
  bool key_frame( void ) const { return key_frame_; }

  const Chunk & first_partition( void ) const { return first_partition_; }
  const std::vector< Chunk > dct_partitions( const uint8_t num ) const;

  LoopFilterType loop_filter_type( void ) const { return loop_filter_; }
  bool show_frame( void ) const { return show_frame_; }

  bool experimental( void ) const { return experimental_; }
  bool reference_update( void ) const { return reference_update_; }
};

#endif /* UNCOMPRESSED_CHUNK_HH */
