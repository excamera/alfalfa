/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef UNCOMPRESSED_CHUNK_HH
#define UNCOMPRESSED_CHUNK_HH

#include "chunk.hh"
#include "loopfilter.hh"

#include <vector>

enum CorruptionLevel
{
  NO_CORRUPTION,
  CORRUPTED_RESIDUES,
  CORRUPTED_FIRST_PARTITION,
  CORRUPTED_FRAME
};

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
  Chunk first_partition_;
  Chunk rest_;
  CorruptionLevel corruption_level_;

public:
  UncompressedChunk( const Chunk & frame, const uint16_t expected_width,
                     const uint16_t expected_height, const bool accept_partial );
  bool key_frame( void ) const { return key_frame_; }

  const Chunk & first_partition( void ) const { return first_partition_; }
  const std::vector< Chunk > dct_partitions( const uint8_t num ) const;

  LoopFilterType loop_filter_type( void ) const { return loop_filter_; }
  bool show_frame( void ) const { return show_frame_; }
  bool experimental( void ) const { return experimental_; }

  CorruptionLevel corruption_level() const { return corruption_level_; }
};

#endif /* UNCOMPRESSED_CHUNK_HH */
