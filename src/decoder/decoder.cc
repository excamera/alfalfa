#include "decoder.hh"
#include "uncompressed_chunk.hh"
#include "frame.hh"

using namespace std;

Decoder::Decoder( uint16_t s_width, uint16_t s_height, const Chunk & key_frame )
  : width_( s_width ), height_( s_height ),
    state_( KeyFrame( UncompressedChunk( key_frame, width_, height_ ), width_, height_ ).header() )
{}

bool Decoder::decode_frame( const Chunk & frame, Raster & raster )
{
  /* parse uncompressed data chunk */
  UncompressedChunk uncompressed_chunk( frame, width_, height_ );

  /* only parse key frames for now */
  if ( !uncompressed_chunk.key_frame() ) {
    //    Interframe myframe( uncompressed_chunk, width_, height_ );

    return false;
  }

  KeyFrame myframe( uncompressed_chunk, width_, height_ );
  state_ = DecoderState( myframe.header() );

  myframe.parse_macroblock_headers( state_ );
  myframe.parse_tokens( state_ );

  myframe.decode( state_, raster );

  return true;
}
