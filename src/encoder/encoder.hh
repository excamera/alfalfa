#ifndef ENCODER_HH
#define ENCODER_HH

#include "decoder.hh"

class Encoder
{
private:
  uint16_t width_, height_;

  DecoderState state_;  

public:
  Encoder( const uint16_t width, const uint16_t height, const Chunk & key_frame );

  std::vector< uint8_t > encode_frame( const Chunk & frame );
};

#endif /* DECODER_HH */
