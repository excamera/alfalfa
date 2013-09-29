#ifndef VP8_PARSER_HH
#define VP8_PARSER_HH

#include "block.hh"

class VP8Parser
{
private:
  uint16_t width, height;

public:
  VP8Parser( uint16_t s_width, uint16_t s_height );

  void parse_frame( const Block & frame );
};

#endif /* VP8_PARSER_HH */
