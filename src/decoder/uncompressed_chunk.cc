#include "uncompressed_chunk.hh"
#include "exception.hh"

using namespace std;

UncompressedChunk::UncompressedChunk( const Chunk & frame,
				      const uint16_t expected_width,
				      const uint16_t expected_height )
  : key_frame_(),
    reconstruction_filter_(),
    loop_filter_(),
    show_frame_(),
    experimental_(),
    reference_update_(),
    first_partition_( nullptr, 0 ),
    rest_( nullptr, 0 )
{
  try {
    /* flags */
    key_frame_ = not frame.bits( 0, 1 );
    show_frame_ = frame.bits( 4, 1 );

    /* advisory filter parameters */
    /* these have no effect on the decoding process */
    const uint8_t version = frame.bits( 1, 3 );
    switch ( version ) {
    case 0:
      reconstruction_filter_ = ReconstructionFilterType::Bicubic;
      loop_filter_ = LoopFilterType::Normal;
      experimental_ = false;
      break;
    case 4:
      reconstruction_filter_ = ReconstructionFilterType::Bicubic;
      loop_filter_ = LoopFilterType::Normal;
      experimental_ = true;
      reference_update_ = false;
      break;
    case 6:
      reconstruction_filter_ = ReconstructionFilterType::Bicubic;
      loop_filter_ = LoopFilterType::NoFilter;
      experimental_ = true;
      reference_update_ = true;
      break;
    default:
      throw Unsupported( "VP8 version of " + to_string( version ) );
    }

    /* If version = 3, this affects the decoding process by truncating
       chroma motion vectors to full pixels. */

    /* first partition */
    uint32_t first_partition_length = frame.bits( 5, 19 );
    uint32_t first_partition_byte_offset = key_frame_ ? 10 : 3;

    if ( not ( experimental_ and not reference_update_ ) ) {
      if ( frame.size() <= first_partition_byte_offset + first_partition_length ) {
        throw Invalid( "invalid VP8 first partition length" );
      }
    }

    first_partition_ = frame( first_partition_byte_offset, first_partition_length );
    rest_ = frame( first_partition_byte_offset + first_partition_length,
		   frame.size() - first_partition_byte_offset - first_partition_length );

    if ( key_frame_ ) {
      if ( frame( 3, 3 ).to_string() != "\x9d\x01\x2a" ) {
	throw Invalid( "did not find key-frame start code" );
      }

      Chunk sizes = frame( 6, 4 );

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

const vector< Chunk > UncompressedChunk::dct_partitions( const uint8_t num ) const
{
  /* extract the rest of the partitions */
  Chunk rest_of_frame = rest_;

  /* get the lengths of all DCT partitions except the last one */
  vector< uint32_t > partition_lengths;
  for ( uint8_t i = 0; i < num - 1; i++ ) {
    partition_lengths.push_back( rest_of_frame.bits( 0, 24 ) );
    rest_of_frame = rest_of_frame( 3 );
  }

  /* make all the DCT partitions */
  vector< Chunk > dct_partitions;
  for ( const auto & length : partition_lengths ) {
    dct_partitions.emplace_back( rest_of_frame( 0, length ) );
    rest_of_frame = rest_of_frame( length );
  }

  /* make the last DCT partition */
  dct_partitions.emplace_back( rest_of_frame );

  return dct_partitions;
}
