#ifndef UNCOMPRESSED_CHUNK_HH
#define UNCOMPRESSED_CHUNK_HH

#include "chunk.hh"
#include "bool_decoder.hh"

#include <vector>

class UncompressedChunk
{
private:
  enum class ReconstructionFilter : char {
    Bicubic,
    Bilinear,
    None
  };

  enum class LoopFilter : char {
    Normal,
    Simple,
    None
  };

  bool key_frame_;
  ReconstructionFilter reconstruction_filter_;
  LoopFilter loop_filter_;
  bool show_frame_;
  Chunk first_partition_;
  Chunk rest_;

public:
  UncompressedChunk( const Chunk & frame, const uint16_t expected_width, const uint16_t expected_height );
  bool key_frame( void ) const { return key_frame_; }

  BoolDecoder first_partition( void ) const { return first_partition_; }
  std::vector< BoolDecoder > dct_partitions( const uint8_t num ) const;
};

#endif /* UNCOMPRESSED_CHUNK_HH */
