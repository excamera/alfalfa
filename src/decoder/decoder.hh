#ifndef DECODER_HH
#define DECODER_HH

#include "chunk.hh"
#include "bool_decoder.hh"
#include "raster.hh"

class Decoder
{
private:
  uint16_t width_, height_;

public:
  Decoder( uint16_t s_width, uint16_t s_height );

  bool decode_frame( const Chunk & frame, Raster & raster );

  unsigned int raster_width( void ) const;
  unsigned int raster_height( void ) const;
};

#endif /* DECODER_HH */
