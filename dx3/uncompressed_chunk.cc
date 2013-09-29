#include "uncompressed_chunk.hh"
#include "exception.hh"

using namespace std;

UncompressedChunk::UncompressedChunk( const Block & frame,
				      const uint16_t expected_width,
				      const uint16_t expected_height )
  : key_frame_(),
    reconstruction_filter_(),
    loop_filter_(),
    show_frame_(),
    first_partition_( nullptr, 0 )
{
  try {
    /* flags */
    key_frame_ = not frame.bits( 0, 1 );
    show_frame_ = frame.bits( 4, 1 );

    /* filter parameters */
    const uint8_t version = frame.bits( 1, 3 );
    switch ( version ) {
    case 0:
      reconstruction_filter_ = ReconstructionFilter::Bicubic;
      loop_filter_ = LoopFilter::Normal;
      break;
    case 1:
      reconstruction_filter_ = ReconstructionFilter::Bilinear;
      loop_filter_ = LoopFilter::Simple;
      break;
    case 2:
      reconstruction_filter_ = ReconstructionFilter::Bilinear;
      loop_filter_ = LoopFilter::None;
      break;
    case 3:
      reconstruction_filter_ = ReconstructionFilter::None;
      loop_filter_ = LoopFilter::None;
      break;
    default:
      throw Unsupported( "VP8 version of " + to_string( version ) );
    }

    /* first partition */
    uint32_t first_partition_length = frame.bits( 5, 19 );
    uint32_t first_partition_byte_offset = key_frame_ ? 10 : 3;

    if ( frame.size() <= first_partition_byte_offset + first_partition_length ) {
      /* why <= ? matches dixie */
      throw Invalid( "invalid VP8 first partition length" );
    }

    if ( key_frame_ ) {
      if ( frame( 3, 3 ).to_string() != "\x9d\x01\x2a" ) {
	throw Invalid( "did not find key-frame start code" );
      }

      Block sizes = frame( 6, 4 );

      const char horizontal_scale = sizes.bits( 14, 2 ), vertical_scale = sizes.bits( 30, 2 );
      const uint16_t frame_width = sizes.bits( 0, 14 ), frame_height = sizes.bits( 16, 14 );

      if ( frame_width != expected_width or frame_height != expected_height
	   or horizontal_scale or vertical_scale ) {
	throw Unsupported( "VP8 upscaling not supported" );
      }
    }
  } catch ( const out_of_range & e ) {
    throw Invalid( "VP8 frame truncated" );
  }
}
