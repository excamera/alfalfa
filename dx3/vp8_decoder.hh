#ifndef VP8_DECODER_HH
#define VP8_DECODER_HH

#include "block.hh"

class VP8Decoder
{
private:
  uint16_t width, height;

public:
  VP8Decoder( uint16_t s_width, uint16_t s_height );

  void decode_frame( const Block & frame );
};

#endif /* VP8_DECODER_HH */
