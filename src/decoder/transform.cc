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

#include "macroblock.hh"
#include "block.hh"
#include "safe_array.hh"
#include "transform_sse.hh"
#include "dct_sse.hh"

template <>
void YBlock::set_dc_coefficient( const int16_t & val )
{
  coefficients_.set_dc_coefficient( val );
  set_Y_without_Y2();
}

void DCTCoefficients::set_dc_coefficient( const int16_t & val )
{
  coefficients_.at( 0 ) = val;
}

void DCTCoefficients::iwht( SafeArray<SafeArray<DCTCoefficients, 4>, 4> & output ) const
{
#ifdef HAVE_SSE2

  vp8_short_inv_walsh4x4_sse2( &at( 0 ), &output.at( 0 ).at( 0 ).at( 0 ) );

#else

  SafeArray< int16_t, 16 > intermediate;

  for ( size_t i = 0; i < 4; i++ ) {
    int a1 = coefficients_.at( i + 0 ) + coefficients_.at( i + 12 );
    int b1 = coefficients_.at( i + 4 ) + coefficients_.at( i + 8  );
    int c1 = coefficients_.at( i + 4 ) - coefficients_.at( i + 8  );
    int d1 = coefficients_.at( i + 0 ) - coefficients_.at( i + 12 );

    intermediate.at( i + 0  ) = a1 + b1;
    intermediate.at( i + 4  ) = c1 + d1;
    intermediate.at( i + 8  ) = a1 - b1;
    intermediate.at( i + 12 ) = d1 - c1;
  }

  for ( size_t i = 0; i < 4; i++ ) {
    const uint8_t offset = i * 4;
    int a1 = intermediate.at( offset + 0 ) + intermediate.at( offset + 3 );
    int b1 = intermediate.at( offset + 1 ) + intermediate.at( offset + 2 );
    int c1 = intermediate.at( offset + 1 ) - intermediate.at( offset + 2 );
    int d1 = intermediate.at( offset + 0 ) - intermediate.at( offset + 3 );

    int a2 = a1 + b1;
    int b2 = c1 + d1;
    int c2 = a1 - b1;
    int d2 = d1 - c1;

    output.at( i ).at( 0 ).at( 0 ) = ( a2 + 3 ) >> 3;
    output.at( i ).at( 1 ).at( 0 ) = ( b2 + 3 ) >> 3;
    output.at( i ).at( 2 ).at( 0 ) = ( c2 + 3 ) >> 3;
    output.at( i ).at( 3 ).at( 0 ) = ( d2 + 3 ) >> 3;
  }

#endif
}

#ifdef HAVE_SSE2

void DCTCoefficients::idct_add( VP8Raster::Block4 & output ) const
{

  vp8_short_idct4x4llm_mmx( &coefficients_.at( 0 ), &output.at( 0, 0 ), output.stride(), &output.at( 0, 0 ), output.stride() );
}

#else

static inline int MUL_20091( const int a ) { return ((((a)*20091) >> 16) + (a)); }
static inline int MUL_35468( const int a ) { return (((a)*35468) >> 16); }

void DCTCoefficients::idct_add( VP8Raster::Block4 & output ) const
{
  SafeArray< int16_t, 16 > intermediate;

  /* Based on libav/ffmpeg vp8_idct_add_c */

  for ( int i = 0; i < 4; i++ ) {
    int t0 = coefficients_.at( i + 0 ) + coefficients_.at( i + 8 );
    int t1 = coefficients_.at( i + 0 ) - coefficients_.at( i + 8 );
    int t2 = MUL_35468( coefficients_.at( i + 4 ) ) - MUL_20091( coefficients_.at( i + 12 ) );
    int t3 = MUL_20091( coefficients_.at( i + 4 ) ) + MUL_35468( coefficients_.at( i + 12 ) );

    intermediate.at( i * 4 + 0 ) = t0 + t3;
    intermediate.at( i * 4 + 1 ) = t1 + t2;
    intermediate.at( i * 4 + 2 ) = t1 - t2;
    intermediate.at( i * 4 + 3 ) = t0 - t3;
  }

  for ( int i = 0; i < 4; i++ ) {
    int t0 = intermediate.at( i + 0 ) + intermediate.at( i + 8 );
    int t1 = intermediate.at( i + 0 ) - intermediate.at( i + 8 );
    int t2 = MUL_35468( intermediate.at( i + 4 ) ) - MUL_20091( intermediate.at( i + 12 ) );
    int t3 = MUL_20091( intermediate.at( i + 4 ) ) + MUL_35468( intermediate.at( i + 12 ) );

    uint8_t *target = &output.at( 0, i );

    *target = clamp255( *target + ((t0 + t3 + 4) >> 3) );
    target++;
    *target = clamp255( *target + ((t1 + t2 + 4) >> 3) );
    target++;
    *target = clamp255( *target + ((t1 - t2 + 4) >> 3) );
    target++;
    *target = clamp255( *target + ((t0 - t3 + 4) >> 3) );
  }
}
#endif

template <BlockType initial_block_type, class PredictionMode>
void Block< initial_block_type, PredictionMode >::add_residue( VP8Raster::Block4 & output ) const
{
  for ( uint8_t i = 0; i < 16; i++ ) {
    output.at( i % 4, i / 4 ) = clamp255( output.at( i % 4, i / 4 )
                                          + coefficients_.at( zigzag.at( i ) ) );
  }
}
