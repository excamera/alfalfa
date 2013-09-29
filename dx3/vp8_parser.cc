#include "vp8_parser.hh"
#include "exception.hh"
#include "bool_decoder.hh"
#include "uncompressed_chunk.hh"

VP8Parser::VP8Parser( uint16_t s_width, uint16_t s_height )
  : width_( s_width ), height_( s_height )
{
}

void VP8Parser::parse_frame( const Block & frame )
{
  /* parse uncompressed data chunk */
  UncompressedChunk uncompressed_chunk( frame, width_, height_ );

  BoolDecoder partition1( uncompressed_chunk.first_partition() );

  if ( uncompressed_chunk.key_frame() ) {
    if ( partition1.get_uint( 2 ) ) {
      throw Unsupported( "VP8 color_space and clamping_type bits" );
    }
  }

  if ( partition1.get_bit() ) {
    printf( "segmentation enabled\n" );
  }
}
