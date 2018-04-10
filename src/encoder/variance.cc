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

#include "encoder.hh"
#include "sad_sse.hh"

#ifndef HAVE_SSE2

template<unsigned int size>
uint32_t Encoder::sad( const VP8Raster::Block<size> & block,
                       const TwoDSubRange<uint8_t, size, size> & prediction )
{
  uint32_t res = 0;

  for ( size_t i = 0; i < size; i++ ) {
    for ( size_t j = 0; j < size; j++ ) {
      res += abs( block.at( i, j ) - prediction.at( i, j ) );
    }
  }

  return res;
}

template<unsigned int size>
uint32_t Encoder::sse( const VP8Raster::Block<size> & block,
                       const TwoDSubRange<uint8_t, size, size> & prediction )
{
  uint32_t res = 0;

  for ( size_t i = 0; i < size; i++ ) {
    for ( size_t j = 0; j < size; j++ ) {
      int16_t diff = ( block.at( i, j ) - prediction.at( i, j ) );
      res += diff * diff;
    }
  }

  return res;
}

template<unsigned int size>
uint32_t Encoder::variance( const VP8Raster::Block<size> & block,
                            const TwoDSubRange<uint8_t, size, size> & prediction )
{
  uint32_t res = 0;
  int32_t sum = 0;

  for ( size_t i = 0; i < size; i++ ) {
    for ( size_t j = 0; j < size; j++ ) {
      int16_t diff = ( block.at( i, j ) - prediction.at( i, j ) );

      sum += diff;
      res += diff * diff;
    }
  }

  return res - ( ( int64_t)sum * sum ) / ( size * size );
}

#else // SSE2 is supported

#include "variance_sse2.cc"

/* SAD() */
template<>
uint32_t Encoder::sad( const VP8Raster::Block<16> & block,
                       const TwoDSubRange<uint8_t, 16, 16> & prediction )
{
  return vpx_sad16x16_sse2( &block.contents().at( 0, 0 ), block.contents().stride(),
                            &prediction.at( 0, 0 ), prediction.stride() );
}

/* SSE() */
template<>
uint32_t Encoder::sse( const VP8Raster::Block<4> & block,
                       const TwoDSubRange<uint8_t, 4, 4> & prediction )
{
  unsigned int sse;
  get4x4var_sse2( &block.contents().at( 0, 0 ), block.contents().stride(),
                  &prediction.at( 0, 0 ), prediction.stride(),
                  &sse, nullptr );

  return sse;
}

template<>
uint32_t Encoder::sse( const VP8Raster::Block<8> & block,
                       const TwoDSubRange<uint8_t, 8, 8> & prediction )
{
  unsigned int sse;
  vpx_get8x8var_sse2( &block.contents().at( 0, 0 ), block.contents().stride(),
                      &prediction.at( 0, 0 ), prediction.stride(),
                      &sse, nullptr );

  return sse;
}

template<>
uint32_t Encoder::sse( const VP8Raster::Block<16> & block,
                       const TwoDSubRange<uint8_t, 16, 16> & prediction )
{
  unsigned int sse;
  vpx_get16x16var_sse2( &block.contents().at( 0, 0 ), block.contents().stride(),
                        &prediction.at( 0, 0 ), prediction.stride(),
                        &sse, nullptr );

  return sse;
}

/* VARIANCE() */

template<>
uint32_t Encoder::variance( const VP8Raster::Block<4> & block,
                            const TwoDSubRange<uint8_t, 4, 4> & prediction )
{
  unsigned int sse;
  return vpx_variance4x4_sse2( &block.contents().at( 0, 0 ), block.contents().stride(),
                                 &prediction.at( 0, 0 ), prediction.stride(),
                                 &sse );
}

template<>
uint32_t Encoder::variance( const VP8Raster::Block<8> & block,
                            const TwoDSubRange<uint8_t, 8, 8> & prediction )
{
  unsigned int sse;
  return vpx_variance8x8_sse2( &block.contents().at( 0, 0 ), block.contents().stride(),
                                 &prediction.at( 0, 0 ), prediction.stride(),
                                 &sse );
}

template<>
uint32_t Encoder::variance( const VP8Raster::Block<16> & block,
                            const TwoDSubRange<uint8_t, 16, 16> & prediction )
{
  unsigned int sse;
  return vpx_variance16x16_sse2( &block.contents().at( 0, 0 ), block.contents().stride(),
                                 &prediction.at( 0, 0 ), prediction.stride(),
                                 &sse );
}

#endif
