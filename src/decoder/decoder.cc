#include "decoder.hh"
#include "uncompressed_chunk.hh"
#include "frame.hh"

using namespace std;

Decoder::Decoder( uint16_t s_width, uint16_t s_height )
  : width_( s_width ), height_( s_height )
{}

bool Decoder::decode_frame( const Chunk & frame, Raster & raster )
{
  /* parse uncompressed data chunk */
  UncompressedChunk uncompressed_chunk( frame, width_, height_ );

  /* only parse key frames for now */
  if ( !uncompressed_chunk.key_frame() ) {
    return false;
  }

  KeyFrame myframe( uncompressed_chunk, width_, height_ );
  myframe.calculate_probability_tables();
  myframe.parse_macroblock_headers();
  myframe.assign_output_raster( raster );
  myframe.decode();
  myframe.loopfilter();

  return true;
}

unsigned int Decoder::raster_width( void ) const
{
  return 16 * ((width_ + 15) / 16);
}

unsigned int Decoder::raster_height( void ) const
{
  return 16 * ((height_ + 15) / 16);
}
