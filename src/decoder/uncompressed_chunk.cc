/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Copyright 2013-2018 the Alfalfa authors
                       and the Massachusetts Institute of Technology

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

      1. Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "uncompressed_chunk.hh"
#include "exception.hh"

using namespace std;

UncompressedChunk::UncompressedChunk( const Chunk & frame,
                                      const uint16_t expected_width,
                                      const uint16_t expected_height,
                                      const bool accept_partial )
  : key_frame_(),
    reconstruction_filter_(),
    loop_filter_(),
    show_frame_(),
    experimental_(),
    first_partition_( nullptr, 0 ),
    rest_( nullptr, 0 ),
    corruption_level_( NO_CORRUPTION )
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
      break;
    case 6:
      reconstruction_filter_ = ReconstructionFilterType::Bicubic;
      loop_filter_ = LoopFilterType::NoFilter;
      experimental_ = true;
      break;
    default:
      throw Unsupported( "VP8 version of " + to_string( version ) );
    }

    /* If version = 3, this affects the decoding process by truncating
       chroma motion vectors to full pixels. */

    /* first partition */
    uint32_t first_partition_length = frame.bits( 5, 19 );
    uint32_t first_partition_byte_offset = key_frame_ ? 10 : 3;

    if ( frame.size() <= first_partition_byte_offset + first_partition_length ) {
      if ( accept_partial ) {
        corruption_level_ = CORRUPTED_FIRST_PARTITION;
      }
      else {
        throw Invalid( "invalid VP8 first partition length" );
      }
    }

    if ( corruption_level_ == CORRUPTED_FIRST_PARTITION ) {
      first_partition_ = frame( first_partition_byte_offset, frame.size() - first_partition_byte_offset );
    }
    else {
      first_partition_ = frame( first_partition_byte_offset, first_partition_length );

      rest_ = frame( first_partition_byte_offset + first_partition_length,
                     frame.size() - first_partition_byte_offset - first_partition_length );
    }

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
    if ( accept_partial ) {
      corruption_level_ = CORRUPTED_FRAME;

      key_frame_ = false;
      reconstruction_filter_ = ReconstructionFilterType::Bicubic;
      loop_filter_ = LoopFilterType::Normal;
      experimental_ = false;
    }
    else {
      throw Invalid( "VP8 frame truncated" );
    }
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
