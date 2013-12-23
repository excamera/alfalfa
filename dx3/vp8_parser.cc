#include <iostream>
#include <unistd.h>

#include "vp8_parser.hh"
#include "uncompressed_chunk.hh"
#include "frame.hh"
#include "display.hh"

using namespace std;

VP8Parser::VP8Parser( uint16_t s_width, uint16_t s_height )
  : width_( s_width ), height_( s_height )
{
}

void VP8Parser::parse_frame( const Chunk & frame, VideoDisplay & display )
{
  /* parse uncompressed data chunk */
  UncompressedChunk uncompressed_chunk( frame, width_, height_ );

  /* only parse key frames for now */
  if ( !uncompressed_chunk.key_frame() ) {
    return;
  }

  KeyFrame myframe( uncompressed_chunk, width_, height_ );
  myframe.calculate_probability_tables();
  myframe.parse_macroblock_headers();
  myframe.parse_tokens();
  myframe.dequantize();
  myframe.initialize_raster();
  myframe.intra_predict_and_inverse_transform();

  display.draw( myframe.raster() );
  sleep( 1 );
}

unsigned int VP8Parser::raster_width( void ) const
{
  return 16 * ((width_ + 15) / 16);
}

unsigned int VP8Parser::raster_height( void ) const
{
  return 16 * ((height_ + 15) / 16);
}
