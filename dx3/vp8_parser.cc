#include <iostream>

#include "vp8_parser.hh"
#include "exception.hh"
#include "bool_decoder.hh"
#include "uncompressed_chunk.hh"
#include "frame_header.hh"
#include "mb_records.hh"
#include "2d.hh"

using namespace std;

VP8Parser::VP8Parser( uint16_t s_width, uint16_t s_height )
  : width_( s_width ), height_( s_height )
{
}

void VP8Parser::parse_frame( const Block & frame )
{
  /* parse uncompressed data chunk */
  UncompressedChunk uncompressed_chunk( frame, width_, height_ );

  /* only parse key frames for now */
  if ( !uncompressed_chunk.key_frame() ) {
    return;
  }

  /* extract the first partition */
  BoolDecoder partition1( uncompressed_chunk.first_partition() );

  /* parse the frame header */
  KeyFrameHeader frame_header( partition1 );

  /* safety checks for the frame header */
  if ( frame_header.color_space or frame_header.clamping_type ) {
    throw Unsupported( "VP8 color_space and clamping_type bits" );
  }

  /* locate the remaining partitions */
  const uint8_t num_dct_partitions = 1 << frame_header.log2_nbr_of_dct_partitions;
  assert( num_dct_partitions <= 8 );

  vector< BoolDecoder > dct_partitions = extract_dct_partitions( uncompressed_chunk.rest(),
								 num_dct_partitions );

  /* parse macroblock prediction records */
  TwoD< KeyFrameMacroblockHeader > mb_records( (width_ + 15) / 16 + 1,
					       (height_ + 15) / 16 + 1,
					       partition1,
					       frame_header );

}

vector< BoolDecoder > VP8Parser::extract_dct_partitions( const Block & after_first_partition,
							 const uint8_t num_dct_partitions )
{
  /* extract the rest of the partitions */
  Block rest_of_frame = after_first_partition;

  /* get the lengths of all DCT partitions except the last one */
  vector< uint32_t > partition_lengths;
  for ( uint8_t i = 0; i < num_dct_partitions - 1; i++ ) {
    partition_lengths.push_back( rest_of_frame.bits( 0, 24 ) );
    rest_of_frame = rest_of_frame( 3 );
  }

  /* make all the DCT partition */
  vector< BoolDecoder > dct_partitions;
  for ( const auto & length : partition_lengths ) {
    dct_partitions.emplace_back( rest_of_frame( 0, length ) );
    rest_of_frame = rest_of_frame( length );
  }

  /* make the last DCT partition */
  dct_partitions.emplace_back( rest_of_frame );

  return dct_partitions;
}
