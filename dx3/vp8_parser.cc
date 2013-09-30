#include <iostream>

#include "vp8_parser.hh"
#include "exception.hh"
#include "bool_decoder.hh"
#include "uncompressed_chunk.hh"
#include "frame_header.hh"

using namespace std;

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
    KeyFrameHeader frame_header( partition1 );

    if ( frame_header.color_space or frame_header.clamping_type ) {
      throw Unsupported( "VP8 color_space and clamping_type bits" );
    }

    if ( frame_header.mode_lf_adjustments.initialized() ) {
      cout << to_string( frame_header.mode_lf_adjustments->mode_ref_lf_delta_update_flag ) << endl;
    }
  }
}
