#include "vp8_parser.hh"
#include "exception.hh"
#include "bool_decoder.hh"

VP8Parser::VP8Parser( uint16_t s_width, uint16_t s_height )
  : width( s_width ), height( s_height )
{
}

void VP8Parser::parse_frame( const Block & frame )
{
  /* decode uncompressed data chunk */
  Block tag = frame( 0, 3 );
  
  const bool interframe = tag.bits( 0, 1 );
  const uint8_t version = tag.bits( 1, 3 );

  const bool experimental = version > 3;

  if ( experimental ) {
    throw Exception( "VP8", "experimental bitstreams not supported" );
  }

  const bool show_frame = tag.bits( 4, 1 );
  const uint32_t first_partition_length = tag.bits( 5, 19 );
  const uint32_t first_partition_byte_offset = interframe ? 10 : 3;

  if ( not interframe ) {
    if ( frame( 3, 3 ).to_string() != "\x9d\x01\x2a" ) {
      throw Exception( "VP8", "invalid key frame header" );
    }

    Block sizes = frame( 6, 4 );

    const char horizontal_scale = sizes.bits( 14, 2 ), vertical_scale = sizes.bits( 30, 2 );
    const uint16_t frame_width = sizes.bits( 0, 14 ), frame_height = sizes.bits( 16, 14 );

    if ( width != frame_width or height != frame_height
	 or horizontal_scale or vertical_scale ) {
      throw Exception( "VP8", "upscaling not supported" );
    }

    BoolDecoder partition1( frame( first_partition_byte_offset, first_partition_length ) );

    partition1.get_bit();
  }

  printf( "size: %lu, interframe: %u, version: %u, show_frame: %u, size0: %u\n",
	  frame.size(), interframe, version, show_frame, first_partition_length );
}
