/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include "macroblock.hh"
#include "vp8_raster.hh"

#include <emmintrin.h> // SSE2
#include <pmmintrin.h> // SSE3

#ifdef HAVE_SSE2

template <>
void VP8Raster::Block<16>::vertical_predict( TwoDSubRange< uint8_t, 16, 16 > & output ) const
{
  const register __m128i saved_row = _mm_load_si128( reinterpret_cast<const __m128i *>( &( predictors().above_row.at( 0, 0 ) ) ) );

  for ( unsigned int row = 0; row < 16; row++ ) {
    __m128i * row_pointer = reinterpret_cast<__m128i *>( &( output.at( 0, row ) ) );
    _mm_store_si128( row_pointer, saved_row );
  }
}

template <>
void VP8Raster::Block<8>::vertical_predict( TwoDSubRange< uint8_t, 8, 8 > & output ) const
{
  const register __m128i saved_row = _mm_load_si128( reinterpret_cast<const __m128i *>( &( predictors().above_row.at( 0, 0 ) ) ) );

  for ( unsigned int row = 0; row < 8; row++ ) {
    __m128i * row_pointer = reinterpret_cast<__m128i *>( &( output.at( 0, row ) ) );
    _mm_store_si128( row_pointer, saved_row );
  }
}

#endif
