/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cmath>

#include "block.hh"
#include "safe_array.hh"
#include "dct_sse.hh"

void DCTCoefficients::subtract_dct( const VP8Raster::Block4 & block,
                                    const TwoDSubRange< uint8_t, 4, 4 > & prediction )
{
  SafeArray< int16_t, 16 > input;

#if HAVE_SSE2

  vpx_subtract_block_sse2( 4, 4,
                           &input.at( 0 ), 4,
                           &block.contents().at( 0, 0 ), block.contents().stride(),
                           &prediction.at( 0, 0 ), prediction.stride() );
  vp8_short_fdct4x4_sse2( &input.at( 0 ), &at( 0 ), 8 );

#else
  for ( size_t column = 0; column < 4; column++ ) {
    for ( size_t row = 0; row < 4; row++ ) {
      input.at( row * 4 + column ) = block.at( column, row ) - prediction.at( column, row );
    }
  }

  size_t pitch = 8;
  int a1, b1, c1, d1;
  size_t i_offset = 0;
  size_t o_offset = 0;

  for ( size_t i = 0; i < 4; i++ ) {
    a1 = ( input.at( i_offset + 0 ) + input.at( i_offset + 3 ) ) * 8;
    b1 = ( input.at( i_offset + 1 ) + input.at( i_offset + 2 ) ) * 8;
    c1 = ( input.at( i_offset + 1 ) - input.at( i_offset + 2 ) ) * 8;
    d1 = ( input.at( i_offset + 0 ) - input.at( i_offset + 3 ) ) * 8;

    at( o_offset + 0 ) = a1 + b1;
    at( o_offset + 2 ) = a1 - b1;

    at( o_offset + 1 ) = (c1 * 2217 + d1 * 5352 +  14500) >> 12;
    at( o_offset + 3 ) = (d1 * 2217 - c1 * 5352 +   7500) >> 12;

    i_offset += pitch / 2;
    o_offset += 4;
  }

  i_offset = o_offset = 0;

  for ( size_t i = 0; i < 4; i++ ) {
    a1 = at( i_offset + 0 ) + at( i_offset + 12 );
    b1 = at( i_offset + 4 ) + at( i_offset +  8 );
    c1 = at( i_offset + 4 ) - at( i_offset +  8 );
    d1 = at( i_offset + 0 ) - at( i_offset + 12 );

    at( o_offset + 0 )  = ( a1 + b1 + 7 ) >> 4;
    at( o_offset + 8 )  = ( a1 - b1 + 7 ) >> 4;

    at( o_offset +  4 ) = ( ( c1 * 2217 + d1 * 5352 + 12000) >> 16 ) + ( d1 != 0 );
    at( o_offset + 12 ) =   ( d1 * 2217 - c1 * 5352 + 51000) >> 16;

    i_offset++;
    o_offset++;
  }
#endif
}

void DCTCoefficients::wht( SafeArray< int16_t, 16 > & input )
{
#ifdef HAVE_SSE2

  vp8_short_walsh4x4_sse2( &input.at( 0 ), &at( 0 ), 8 );

#else

  size_t pitch = 8;
  int a1, b1, c1, d1;
  int a2, b2, c2, d2;
  size_t i_offset = 0;
  size_t o_offset = 0;

  for ( size_t i = 0; i < 4; i++ ) {
    a1 = ( input.at( i_offset + 0 ) + input.at( i_offset + 2 ) ) * 4;
    d1 = ( input.at( i_offset + 1 ) + input.at( i_offset + 3 ) ) * 4;
    c1 = ( input.at( i_offset + 1 ) - input.at( i_offset + 3 ) ) * 4;
    b1 = ( input.at( i_offset + 0 ) - input.at( i_offset + 2 ) ) * 4;

    at( o_offset + 0 ) = a1 + d1 + ( a1 != 0 );
    at( o_offset + 1 ) = b1 + c1;
    at( o_offset + 2 ) = b1 - c1;
    at( o_offset + 3 ) = a1 - d1;

    i_offset += pitch / 2;
    o_offset += 4;
  }

  i_offset = 0;
  o_offset = 0;

  for ( size_t i = 0; i < 4; i++ ) {
    a1 = at( i_offset + 0 ) + at ( i_offset +  8 );
    d1 = at( i_offset + 4 ) + at ( i_offset + 12 );
    c1 = at( i_offset + 4 ) - at ( i_offset + 12 );
    b1 = at( i_offset + 0 ) - at ( i_offset +  8 );

    a2 = a1 + d1;
    b2 = b1 + c1;
    c2 = b1 - c1;
    d2 = a1 - d1;

    a2 += a2 < 0;
    b2 += b2 < 0;
    c2 += c2 < 0;
    d2 += d2 < 0;

    at( o_offset +  0 ) = ( a2 + 3 ) >> 3;
    at( o_offset +  4 ) = ( b2 + 3 ) >> 3;
    at( o_offset +  8 ) = ( c2 + 3 ) >> 3;
    at( o_offset + 12 ) = ( d2 + 3 ) >> 3;

    i_offset++;
    o_offset++;
  }

#endif
}
