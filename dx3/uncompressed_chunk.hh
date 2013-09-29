#ifndef UNCOMPRESSED_CHUNK_HH
#define UNCOMPRESSED_CHUNK_HH

#include "block.hh"

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
  Block first_partition_;

public:
  UncompressedChunk( const Block & frame, const uint16_t expected_width, const uint16_t expected_height );

  bool key_frame( void ) const { return key_frame_; }
  const Block & first_partition( void ) const { return first_partition_; }
};

#endif /* UNCOMPRESSED_CHUNK_HH */
