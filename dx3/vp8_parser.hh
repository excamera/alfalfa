#ifndef VP8_PARSER_HH
#define VP8_PARSER_HH

#include <vector>

#include "chunk.hh"
#include "bool_decoder.hh"

class VP8Parser
{
private:
  uint16_t width_, height_;

  std::vector< BoolDecoder > extract_dct_partitions( const Chunk & after_first_partition,
						     const uint8_t num_dct_partitions );

public:
  VP8Parser( uint16_t s_width, uint16_t s_height );

  void parse_frame( const Chunk & frame );
};

#endif /* VP8_PARSER_HH */
