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

#include <cmath>
#include <algorithm>

#include "macroblock.hh"
#include "vp8_raster.hh"
#include "intrapred_sse.hh"

using namespace std;

template <unsigned int size>
VP8Raster::Block<size>::Block( const unsigned int column, const unsigned int row,
                               TwoD< uint8_t > & raster_component )
  : contents_( raster_component, size * column, size * row ),
    raster_component_( raster_component ),
    column_( column ),
    row_( row )
{}

VP8Raster::Macroblock::Macroblock( Macroblock && other )
: Y( move( other.Y ) ),
  U( move( other.U ) ),
  V( move( other.V ) ),
  Y_sub( move( other.Y_sub ) ),
  U_sub( move( other.U_sub ) ),
  V_sub( move( other.V_sub ) )
{}

VP8Raster::Macroblock::Macroblock( const TwoD< Macroblock >::Context & c, VP8Raster & raster )
: Macroblock( c.column, c.row, raster )
{}

VP8Raster::Macroblock::Macroblock( const unsigned int column, const unsigned int row,
                                   VP8Raster & raster )
: Y( column, row, raster.Y() ),
  U( column, row, raster.U() ),
  V( column, row, raster.V() ),
  Y_sub( {
      Block4( 4 * column + 0, 4 * row + 0, raster.Y() ),
      Block4( 4 * column + 1, 4 * row + 0, raster.Y() ),
      Block4( 4 * column + 2, 4 * row + 0, raster.Y() ),
      Block4( 4 * column + 3, 4 * row + 0, raster.Y() ),
      Block4( 4 * column + 0, 4 * row + 1, raster.Y() ),
      Block4( 4 * column + 1, 4 * row + 1, raster.Y() ),
      Block4( 4 * column + 2, 4 * row + 1, raster.Y() ),
      Block4( 4 * column + 3, 4 * row + 1, raster.Y() ),
      Block4( 4 * column + 0, 4 * row + 2, raster.Y() ),
      Block4( 4 * column + 1, 4 * row + 2, raster.Y() ),
      Block4( 4 * column + 2, 4 * row + 2, raster.Y() ),
      Block4( 4 * column + 3, 4 * row + 2, raster.Y() ),
      Block4( 4 * column + 0, 4 * row + 3, raster.Y() ),
      Block4( 4 * column + 1, 4 * row + 3, raster.Y() ),
      Block4( 4 * column + 2, 4 * row + 3, raster.Y() ),
      Block4( 4 * column + 3, 4 * row + 3, raster.Y() ) } ),
  U_sub( {
      Block4( 2 * column + 0, 2 * row + 0, raster.U() ),
      Block4( 2 * column + 1, 2 * row + 0, raster.U() ),
      Block4( 2 * column + 0, 2 * row + 1, raster.U() ),
      Block4( 2 * column + 1, 2 * row + 1, raster.U() ) } ),
  V_sub( {
      Block4( 2 * column + 0, 2 * row + 0, raster.V() ),
      Block4( 2 * column + 1, 2 * row + 0, raster.V() ),
      Block4( 2 * column + 0, 2 * row + 1, raster.V() ),
      Block4( 2 * column + 1, 2 * row + 1, raster.V() ) } )
{}

VP8Raster::VP8Raster( const unsigned int display_width, const unsigned int display_height )
: BaseRaster( display_width, display_height,
              16 * macroblock_dimension( display_width ), 16 * macroblock_dimension( display_height ) )
{}

template <unsigned int size>
typename VP8Raster::Block<size>::Predictors VP8Raster::Block<size>::predictors() const
{
  static const uint8_t ROW_127 = 127;
  static const uint8_t COL_129 = 129;

  Predictors predictors_;

  // left predictor
  if ( column_ > 0 ) {
    for ( size_t i = 0; i < size; i++ ) {
      predictors_.left[ i ] = raster_component_.at( size * column_ - 1, size * row_ + i );
    }
  }
  else {
    memset( predictors_.left, COL_129, size );
  }

  // above
  if ( row_ > 0 ) {
    memcpy( predictors_.above, &raster_component_.at( size * column_, size * row_ - 1 ), size );
  }
  else {
    memset( predictors_.above, ROW_127, size );
  }

  // above-left
  if ( column_ > 0 and row_ > 0 ) {
    predictors_.above[ -1 ] = raster_component_.at( size * column_ - 1, size * row_ - 1 );
  }
  else if ( row_ > 0 ) {
    predictors_.above[ -1 ] = COL_129;
  }
  else {
    predictors_.above[ -1 ] = ROW_127;
  }

  if ( size != 4 ) {
    return predictors_; // above-right-bottom-row is not needed.
  }

  // above-right-bottom-row -- only for 4x4 subblocks
  if ( row_ == 0 ) {
    memset( predictors_.above + size, ROW_127, size );
  }
  else if ( size * ( column_ + 1 ) >= raster_component_.width() ) {
    if ( row_ >= 4 ) {
      memset( predictors_.above + size, raster_component_.at( size * ( column_ + 1 ) - 1, size * ( ( row_ / 4 ) * 4 ) - 1 ), size );
    }
    else {
      memset( predictors_.above + size, ROW_127, size );
    }
  }
  else {
    if ( column_ % 4 == 3 and row_ % 4 != 0 ) {
      if ( row_ >= 4 ) {
        memcpy( predictors_.above + size, &raster_component_.at( size * ( column_ + 1 ), size * ( ( row_ / 4 ) * 4 ) - 1 ), size );
      }
      else {
        memset( predictors_.above + size, ROW_127, size );
      }
    }
    else {
      memcpy( predictors_.above + size, &raster_component_.at( size * ( column_ + 1 ), size * row_ - 1 ), size );
    }
  }

  return predictors_;
}

#ifdef HAVE_SSE2

template<>
void VP8Raster::Block<4>::true_motion_predict( const Predictors & predictors,
                                               BlockSubRange & output ) const
{
  vpx_tm_predictor_4x4_sse2( &output.at( 0, 0 ), output.stride(),
                             predictors.above, predictors.left );
}

template<>
void VP8Raster::Block<8>::true_motion_predict( const Predictors & predictors,
                                               BlockSubRange & output ) const
{
  vpx_tm_predictor_8x8_sse2( &output.at( 0, 0 ), output.stride(),
                             predictors.above, predictors.left );
}

template<>
void VP8Raster::Block<16>::true_motion_predict( const Predictors & predictors,
                                                BlockSubRange & output ) const
{
  vpx_tm_predictor_16x16_sse2( &output.at( 0, 0 ), output.stride(),
                               predictors.above, predictors.left );
}

#else

template <unsigned int size>
void VP8Raster::Block<size>::true_motion_predict( const Predictors & predictors,
                                                  BlockSubRange & output ) const
{
  output.forall_ij(
    [&] ( uint8_t & b, unsigned int column, unsigned int row )
    {
      b = clamp255( predictors.left[ row ]
                  + predictors.above[ column ]
                  - predictors.above[ -1 ] );
    }
  );
}

#endif

#ifdef HAVE_SSE2

template<>
void VP8Raster::Block<4>::horizontal_predict( const Predictors & predictors,
                                              BlockSubRange & output ) const
{
  vpx_h_predictor_4x4_sse2( &output.at( 0, 0 ), output.stride(),
                            predictors.above, predictors.left );
}

template<>
void VP8Raster::Block<8>::horizontal_predict( const Predictors & predictors,
                                              BlockSubRange & output ) const
{
  vpx_h_predictor_8x8_sse2( &output.at( 0, 0 ), output.stride(),
                            predictors.above, predictors.left );
}

template<>
void VP8Raster::Block<16>::horizontal_predict( const Predictors & predictors,
                                               BlockSubRange & output ) const
{
  vpx_h_predictor_16x16_sse2( &output.at( 0, 0 ), output.stride(),
                              predictors.above, predictors.left );
}

#else

template <unsigned int size>
void VP8Raster::Block<size>::horizontal_predict( const Predictors & predictors,
                                                 BlockSubRange & output ) const
{
  for ( unsigned int row = 0; row < size; row++ ) {
    output.row( row ).fill( predictors.left[ row ] );
  }
}

#endif

#ifdef HAVE_SSE2

template<>
void VP8Raster::Block<4>::vertical_predict( const Predictors & predictors,
                                            BlockSubRange & output ) const
{
  vpx_v_predictor_4x4_sse2( &output.at( 0, 0 ), output.stride(),
                            predictors.above, predictors.left );
}

template<>
void VP8Raster::Block<8>::vertical_predict( const Predictors & predictors,
                                            BlockSubRange & output ) const
{
  vpx_v_predictor_8x8_sse2( &output.at( 0, 0 ), output.stride(),
                            predictors.above, predictors.left );
}

template<>
void VP8Raster::Block<16>::vertical_predict( const Predictors & predictors,
                                             BlockSubRange & output ) const
{
  vpx_v_predictor_16x16_sse2( &output.at( 0, 0 ), output.stride(),
                            predictors.above, predictors.left );
}

#else

template <unsigned int size>
void VP8Raster::Block<size>::vertical_predict( const Predictors & predictors,
                                               BlockSubRange & output ) const
{
  for ( unsigned int column = 0; column < size; column++ ) {
    output.column( column ).fill( predictors.above[ column ] );
  }
}

#endif

#ifdef HAVE_SSE2

template<>
void VP8Raster::Block<4>::dc_predict_simple( const Predictors & predictors,
                                             BlockSubRange & output ) const
{
  vpx_dc_predictor_4x4_sse2( &output.at( 0, 0 ), output.stride(),
                             predictors.above, predictors.left );
}

template<>
void VP8Raster::Block<8>::dc_predict_simple( const Predictors & predictors,
                                             BlockSubRange & output ) const
{
  vpx_dc_predictor_8x8_sse2( &output.at( 0, 0 ), output.stride(),
                             predictors.above, predictors.left );
}

template<>
void VP8Raster::Block<16>::dc_predict_simple( const Predictors & predictors,
                                              BlockSubRange & output ) const
{
  vpx_dc_predictor_16x16_sse2( &output.at( 0, 0 ), output.stride(),
                               predictors.above, predictors.left );
}

template<>
void VP8Raster::Block<4>::dc_predict( const Predictors & predictors,
                                      BlockSubRange & output ) const
{
  if ( column_ and row_ ) {
    return dc_predict_simple( predictors, output );
  }

  if ( row_ > 0 ) {
    return vpx_dc_top_predictor_4x4_sse2( &output.at( 0, 0 ), output.stride(),
                                          predictors.above, predictors.left );
  }

  if ( column_ > 0 ) {
    return vpx_dc_left_predictor_4x4_sse2( &output.at( 0, 0 ), output.stride(),
                                           predictors.above, predictors.left );
  }

  return vpx_dc_128_predictor_4x4_sse2( &output.at( 0, 0 ), output.stride(),
                                        predictors.above, predictors.left );
}

template<>
void VP8Raster::Block<8>::dc_predict( const Predictors & predictors,
                                      BlockSubRange & output ) const
{
  if ( column_ and row_ ) {
    return dc_predict_simple( predictors, output );
  }

  if ( row_ > 0 ) {
    return vpx_dc_top_predictor_8x8_sse2( &output.at( 0, 0 ), output.stride(),
                                          predictors.above, predictors.left );
  }

  if ( column_ > 0 ) {
    return vpx_dc_left_predictor_8x8_sse2( &output.at( 0, 0 ), output.stride(),
                                           predictors.above, predictors.left );
  }

  return vpx_dc_128_predictor_8x8_sse2( &output.at( 0, 0 ), output.stride(),
                                        predictors.above, predictors.left );
}

template<>
void VP8Raster::Block<16>::dc_predict( const Predictors & predictors,
                                       BlockSubRange & output ) const
{
  if ( column_ and row_ ) {
    return dc_predict_simple( predictors, output );
  }

  if ( row_ > 0 ) {
    return vpx_dc_top_predictor_16x16_sse2( &output.at( 0, 0 ), output.stride(),
                                            predictors.above, predictors.left );
  }

  if ( column_ > 0 ) {
    return vpx_dc_left_predictor_16x16_sse2( &output.at( 0, 0 ), output.stride(),
                                             predictors.above, predictors.left );
  }

  return vpx_dc_128_predictor_16x16_sse2( &output.at( 0, 0 ), output.stride(),
                                          predictors.above, predictors.left );
}

#else

template <unsigned int size>
void VP8Raster::Block<size>::dc_predict_simple( const Predictors & predictors,
                                                BlockSubRange & output ) const
{
  static_assert( size == 4 or size == 8 or size == 16, "invalid Block size" );
  static constexpr uint8_t log2size = size == 4 ? 2 : size == 8 ? 3 : size == 16 ? 4 : 0;

  int16_t above_sum = 0;
  int16_t left_sum = 0;

  for ( size_t i = 0; i < size; i++ ) {
    above_sum += predictors.above[ i ];
    left_sum += predictors.left[ i ];
  }

  uint8_t value = ( above_sum + left_sum + ( 1 << log2size ) ) >> ( log2size + 1 );

  output.fill( value );
}

template <unsigned int size>
void VP8Raster::Block<size>::dc_predict( const Predictors & predictors,
                                         BlockSubRange & output ) const
{
  if ( column_ and row_ ) {
    return dc_predict_simple( predictors, output );
  }

  uint8_t value = 128;
  static_assert( size == 4 or size == 8 or size == 16, "invalid Block size" );
  static constexpr uint8_t log2size = size == 4 ? 2 : size == 8 ? 3 : size == 16 ? 4 : 0;

  if ( row_ > 0 ) {
    int16_t above_sum = 0;
    for ( size_t i = 0; i < size; i++ ) { above_sum += predictors.above[ i ]; }

    value = ( above_sum + ( 1 << ( log2size - 1 ) ) ) >> log2size;
  }
  else if ( column_ > 0 ) {
    int16_t left_sum = 0;
    for ( size_t i = 0; i < size; i++ ) { left_sum += predictors.left[ i ]; }

    value = ( left_sum + ( 1 << ( log2size - 1 ) ) ) >> log2size;
  }

  output.fill( value );
}

#endif

template <>
template <>
void VP8Raster::Block8::intra_predict( const mbmode uv_mode,
                                       const Predictors & predictors,
                                       BlockSubRange & output ) const
{
  /* Chroma prediction */

  switch ( uv_mode ) {
  case DC_PRED: dc_predict( predictors, output ); break;
  case V_PRED:  vertical_predict( predictors, output );  break;
  case H_PRED:  horizontal_predict( predictors, output );  break;
  case TM_PRED: true_motion_predict( predictors, output ); break;
  default: throw LogicError(); /* tree decoder for uv_mode can't produce this */
  }
}

template <>
template <>
void VP8Raster::Block16::intra_predict( const mbmode uv_mode,
                                        const Predictors & predictors,
                                        BlockSubRange & output ) const
{
  /* Y prediction for whole macroblock */

  switch ( uv_mode ) {
  case DC_PRED: dc_predict( predictors, output ); break;
  case V_PRED:  vertical_predict( predictors, output );  break;
  case H_PRED:  horizontal_predict( predictors, output );  break;
  case TM_PRED: true_motion_predict( predictors, output ); break;
  default: throw LogicError(); /* need to predict and transform subblocks independently */
  }
}

uint8_t avg3( const uint8_t x, const uint8_t y, const uint8_t z )
{
  return (x + 2 * y + z + 2) >> 2;
}

uint8_t avg2( const uint8_t x, const uint8_t y )
{
  return (x + y + 1) >> 1;
}

template <>
void VP8Raster::Block4::vertical_smoothed_predict( const Predictors & predictors,
                                                   BlockSubRange & output ) const
{
  output.column( 0 ).fill( avg3( predictors.above[ -1 ], predictors.above[ 0 ], predictors.above[ 1 ] ) );
  output.column( 1 ).fill( avg3( predictors.above[ 0 ],  predictors.above[ 1 ], predictors.above[ 2 ] ) );
  output.column( 2 ).fill( avg3( predictors.above[ 1 ],  predictors.above[ 2 ], predictors.above[ 3 ] ) );
  output.column( 3 ).fill( avg3( predictors.above[ 2 ],  predictors.above[ 3 ], predictors.above[ 4 ] ) );
}

template <>
void VP8Raster::Block4::horizontal_smoothed_predict( const Predictors & predictors,
                                                     BlockSubRange & output ) const
{
  output.row( 0 ).fill( avg3( predictors.above[ -1 ], predictors.left[ 0 ], predictors.left[ 1 ] ) );
  output.row( 1 ).fill( avg3( predictors.left[ 0 ],   predictors.left[ 1 ], predictors.left[ 2 ] ) );
  output.row( 2 ).fill( avg3( predictors.left[ 1 ],   predictors.left[ 2 ], predictors.left[ 3 ] ) );
  output.row( 3 ).fill( avg3( predictors.left[ 2 ],   predictors.left[ 3 ], predictors.left[ 3 ] ) );
  /* last line is special because we can't use left( 4 ) yet */
}

template <>
void VP8Raster::Block4::left_down_predict( const Predictors & predictors,
                                           BlockSubRange & output ) const
{
  uint8_t * above = predictors.above;

  output.at( 0, 0 ) =                                                             avg3( above[ 0 ], above[ 1 ], above[ 2 ] );
  output.at( 1, 0 ) = output.at( 0, 1 ) =                                         avg3( above[ 1 ], above[ 2 ], above[ 3 ] );
  output.at( 2, 0 ) = output.at( 1, 1 ) = output.at( 0, 2 ) =                     avg3( above[ 2 ], above[ 3 ], above[ 4 ] );
  output.at( 3, 0 ) = output.at( 2, 1 ) = output.at( 1, 2 ) = output.at( 0, 3 ) = avg3( above[ 3 ], above[ 4 ], above[ 5 ] );
  output.at( 3, 1 ) = output.at( 2, 2 ) = output.at( 1, 3 ) =                     avg3( above[ 4 ], above[ 5 ], above[ 6 ] );
  output.at( 3, 2 ) = output.at( 2, 3 ) =                                         avg3( above[ 5 ], above[ 6 ], above[ 7 ] );
  output.at( 3, 3 ) =                                                             avg3( above[ 6 ], above[ 7 ], above[ 7 ] );
  /* last line is special because we don't use above( 8 ) */
}

template <>
void VP8Raster::Block4::right_down_predict( const Predictors & predictors,
                                            BlockSubRange & output ) const
{
  output.at( 0, 3 ) =                                                             avg3( predictors.east( 0 ), predictors.east( 1 ), predictors.east( 2 ) );
  output.at( 1, 3 ) = output.at( 0, 2 ) =                                         avg3( predictors.east( 1 ), predictors.east( 2 ), predictors.east( 3 ) );
  output.at( 2, 3 ) = output.at( 1, 2 ) = output.at( 0, 1 ) =                     avg3( predictors.east( 2 ), predictors.east( 3 ), predictors.east( 4 ) );
  output.at( 3, 3 ) = output.at( 2, 2 ) = output.at( 1, 1 ) = output.at( 0, 0 ) = avg3( predictors.east( 3 ), predictors.east( 4 ), predictors.east( 5 ) );
  output.at( 3, 2 ) = output.at( 2, 1 ) = output.at( 1, 0 ) =                     avg3( predictors.east( 4 ), predictors.east( 5 ), predictors.east( 6 ) );
  output.at( 3, 1 ) = output.at( 2, 0 ) =                                         avg3( predictors.east( 5 ), predictors.east( 6 ), predictors.east( 7 ) );
  output.at( 3, 0 ) =                                                             avg3( predictors.east( 6 ), predictors.east( 7 ), predictors.east( 8 ) );
}

template <>
void VP8Raster::Block4::vertical_right_predict( const Predictors & predictors,
                                                BlockSubRange & output ) const
{
  output.at( 0, 3 ) =                     avg3( predictors.east( 1 ), predictors.east( 2 ), predictors.east( 3 ) );
  output.at( 0, 2 ) =                     avg3( predictors.east( 2 ), predictors.east( 3 ), predictors.east( 4 ) );
  output.at( 1, 3 ) = output.at( 0, 1 ) = avg3( predictors.east( 3 ), predictors.east( 4 ), predictors.east( 5 ) );
  output.at( 1, 2 ) = output.at( 0, 0 ) = avg2( predictors.east( 4 ), predictors.east( 5 ) );
  output.at( 2, 3 ) = output.at( 1, 1 ) = avg3( predictors.east( 4 ), predictors.east( 5 ), predictors.east( 6 ) );
  output.at( 2, 2 ) = output.at( 1, 0 ) = avg2( predictors.east( 5 ), predictors.east( 6 ) );
  output.at( 3, 3 ) = output.at( 2, 1 ) = avg3( predictors.east( 5 ), predictors.east( 6 ), predictors.east( 7 ) );
  output.at( 3, 2 ) = output.at( 2, 0 ) = avg2( predictors.east( 6 ), predictors.east( 7 ) );
  output.at( 3, 1 ) =                     avg3( predictors.east( 6 ), predictors.east( 7 ), predictors.east( 8 ) );
  output.at( 3, 0 ) =                     avg2( predictors.east( 7 ), predictors.east( 8 ) );
}

template <>
void VP8Raster::Block4::vertical_left_predict( const Predictors & predictors,
                                               BlockSubRange & output ) const
{
  output.at( 0, 0 ) =                     avg2( predictors.above[ 0 ], predictors.above[ 1 ] );
  output.at( 0, 1 ) =                     avg3( predictors.above[ 0 ], predictors.above[ 1 ], predictors.above[ 2 ] );
  output.at( 0, 2 ) = output.at( 1, 0 ) = avg2( predictors.above[ 1 ], predictors.above[ 2 ] );
  output.at( 1, 1 ) = output.at( 0, 3 ) = avg3( predictors.above[ 1 ], predictors.above[ 2 ], predictors.above[ 3 ] );
  output.at( 1, 2 ) = output.at( 2, 0 ) = avg2( predictors.above[ 2 ], predictors.above[ 3 ] );
  output.at( 1, 3 ) = output.at( 2, 1 ) = avg3( predictors.above[ 2 ], predictors.above[ 3 ], predictors.above[ 4 ] );
  output.at( 2, 2 ) = output.at( 3, 0 ) = avg2( predictors.above[ 3 ], predictors.above[ 4 ] );
  output.at( 2, 3 ) = output.at( 3, 1 ) = avg3( predictors.above[ 3 ], predictors.above[ 4 ], predictors.above[ 5 ] );
  output.at( 3, 2 ) =                     avg3( predictors.above[ 4 ], predictors.above[ 5 ], predictors.above[ 6 ] );
  output.at( 3, 3 ) =                     avg3( predictors.above[ 5 ], predictors.above[ 6 ], predictors.above[ 7 ] );
}

#ifdef HAVE_SSE2

template <>
void VP8Raster::Block4::horizontal_down_predict( const Predictors & predictors,
                                                 BlockSubRange & output ) const
{
  vpx_d153_predictor_4x4_ssse3( &output.at( 0, 0 ), output.stride(),
                                predictors.above, predictors.left );
}

#else

template <>
void VP8Raster::Block4::horizontal_down_predict( const Predictors & predictors,
                                                 BlockSubRange & output ) const
{
  output.at( 0, 3 ) =                     avg2( predictors.east( 0 ), predictors.east( 1 ) );
  output.at( 1, 3 ) =                     avg3( predictors.east( 0 ), predictors.east( 1 ), predictors.east( 2 ) );
  output.at( 0, 2 ) = output.at( 2, 3 ) = avg2( predictors.east( 1 ), predictors.east( 2 ) );
  output.at( 1, 2 ) = output.at( 3, 3 ) = avg3( predictors.east( 1 ), predictors.east( 2 ), predictors.east( 3 ) );
  output.at( 2, 2 ) = output.at( 0, 1 ) = avg2( predictors.east( 2 ), predictors.east( 3 ) );
  output.at( 3, 2 ) = output.at( 1, 1 ) = avg3( predictors.east( 2 ), predictors.east( 3 ), predictors.east( 4 ) );
  output.at( 2, 1 ) = output.at( 0, 0 ) = avg2( predictors.east( 3 ), predictors.east( 4 ) );
  output.at( 3, 1 ) = output.at( 1, 0 ) = avg3( predictors.east( 3 ), predictors.east( 4 ), predictors.east( 5 ) );
  output.at( 2, 0 ) =                     avg3( predictors.east( 4 ), predictors.east( 5 ), predictors.east( 6 ) );
  output.at( 3, 0 ) =                     avg3( predictors.east( 5 ), predictors.east( 6 ), predictors.east( 7 ) );
}

#endif

#ifdef HAVE_SSE2

template <>
void VP8Raster::Block4::horizontal_up_predict( const Predictors & predictors,
                                               BlockSubRange & output ) const
{
  vpx_d207_predictor_4x4_sse2( &output.at( 0, 0 ), output.stride(),
                               predictors.above, predictors.left );
}

#else

template <>
void VP8Raster::Block4::horizontal_up_predict( const Predictors & predictors,
                                               BlockSubRange & output ) const
{
  output.at( 0, 0 ) =                     avg2( predictors.left[ 0 ], predictors.left[ 1 ] );
  output.at( 1, 0 ) =                     avg3( predictors.left[ 0 ], predictors.left[ 1 ], predictors.left[ 2 ] );
  output.at( 2, 0 ) = output.at( 0, 1 ) = avg2( predictors.left[ 1 ], predictors.left[ 2 ] );
  output.at( 3, 0 ) = output.at( 1, 1 ) = avg3( predictors.left[ 1 ], predictors.left[ 2 ], predictors.left[ 3 ] );
  output.at( 2, 1 ) = output.at( 0, 2 ) = avg2( predictors.left[ 2 ], predictors.left[ 3 ] );
  output.at( 3, 1 ) = output.at( 1, 2 ) = avg3( predictors.left[ 2 ], predictors.left[ 3 ], predictors.left[ 3 ] );
  output.at( 2, 2 ) = output.at( 3, 2 )
                    = output.at( 0, 3 )
                    = output.at( 1, 3 )
                    = output.at( 2, 3 )
                    = output.at( 3, 3 ) = predictors.left[ 3 ];
}

#endif

template <>
template <>
void VP8Raster::Block4::intra_predict( const bmode b_mode,
                                       const Predictors & predictors,
                                       TwoDSubRange< uint8_t, 4, 4 > & output ) const
{
  /* Luma prediction */

  switch ( b_mode ) {
  case B_DC_PRED: dc_predict_simple( predictors, output ); break;
  case B_TM_PRED: true_motion_predict( predictors, output ); break;
  case B_VE_PRED: vertical_smoothed_predict( predictors, output ); break;
  case B_HE_PRED: horizontal_smoothed_predict( predictors, output ); break;
  case B_LD_PRED: left_down_predict( predictors, output ); break;
  case B_RD_PRED: right_down_predict( predictors, output ); break;
  case B_VR_PRED: vertical_right_predict( predictors, output ); break;
  case B_VL_PRED: vertical_left_predict( predictors, output ); break;
  case B_HD_PRED: horizontal_down_predict( predictors, output ); break;
  case B_HU_PRED: horizontal_up_predict( predictors, output ); break;
  default: throw LogicError();
  }
}

static constexpr SafeArray<SafeArray<int16_t, 6>, 8> sixtap_filters =
  {{ { { 0,  0,  128,    0,   0,  0 } },
     { { 0, -6,  123,   12,  -1,  0 } },
     { { 2, -11, 108,   36,  -8,  1 } },
     { { 0, -9,   93,   50,  -6,  0 } },
     { { 3, -16,  77,   77, -16,  3 } },
     { { 0, -6,   50,   93,  -9,  0 } },
     { { 1, -8,   36,  108, -11,  2 } },
     { { 0, -1,   12,  123,  -6,  0 } } }};

template <unsigned int size>
void VP8Raster::Block<size>::inter_predict( const MotionVector & mv,
                                            const TwoD<uint8_t> & reference,
                                            TwoDSubRange<uint8_t, size, size> & output ) const
{
  const int source_column = column_ * size + ( mv.x() >> 3 );
  const int source_row = row_ * size + ( mv.y() >> 3 );

  if ( source_column - 2 < 0
       or source_column + size + 3 > reference.width()
       or source_row - 2 < 0
       or source_row + size + 3 > reference.height() ) {

    EdgeExtendedRaster safe_reference( reference );

    safe_inter_predict( mv, safe_reference, source_column, source_row, output );
  } else {
    unsafe_inter_predict( mv, reference, source_column, source_row, output );
  }
}

template void VP8Raster::Block<16>::inter_predict( const MotionVector & mv,
                                                   const TwoD<uint8_t> & reference,
                                                   TwoDSubRange<uint8_t, 16, 16> & output ) const;

#ifdef HAVE_SSE2

template <unsigned int size>
void VP8Raster::Block<size>::inter_predict( const MotionVector & mv,
                                            const SafeRaster & reference,
                                            TwoDSubRange<uint8_t, size, size> & output ) const
{
  const int source_column = column_ * size + ( mv.x() >> 3 );
  const int source_row = row_ * size + ( mv.y() >> 3 );

  const unsigned int dst_stride = output.stride();
  const unsigned int src_stride = reference.stride();

  const uint8_t mx = mv.x() & 7, my = mv.y() & 7;

  if ( (mx & 7) == 0 and (my & 7) == 0 ) {
    uint8_t *dest_row_start = &output.at( 0, 0 );
    const uint8_t *src_row_start = &reference.at( source_column, source_row );
    const uint8_t *dest_last_row_start = dest_row_start + size * dst_stride;
    while ( dest_row_start != dest_last_row_start ) {
      memcpy( dest_row_start, src_row_start, size );
      dest_row_start += dst_stride;
      src_row_start += src_stride;
    }
    return;
  }

  alignas(16) SafeArray< SafeArray< uint8_t, size + 8 >, size + 8 > intermediate;
  const uint8_t *intermediate_ptr = &intermediate.at( 0 ).at( 0 );
  const uint8_t *src_ptr = &reference.at( source_column, source_row );
  const uint8_t *dst_ptr = &output.at( 0, 0 );

  if ( mx ) {
    if ( my ) {
      sse_horiz_inter_predict( src_ptr - 2 * src_stride, src_stride, intermediate_ptr,
                               size, size + 5, mx );
      sse_vert_inter_predict( intermediate_ptr, size, dst_ptr, dst_stride,
                              size, my );
    }
    else {
      /* First pass only */
      sse_horiz_inter_predict( src_ptr, src_stride, dst_ptr, dst_stride, size, mx );
    }
  }
  else {
    /* Second pass only */
    sse_vert_inter_predict( src_ptr - 2 * src_stride, src_stride, dst_ptr, dst_stride,
                            size, my );
  }
}

template
void VP8Raster::Block<16>::inter_predict( const MotionVector & mv,
                                            const SafeRaster & reference,
                                            TwoDSubRange<uint8_t, 16, 16> & output ) const;

#endif

#ifdef HAVE_SSE2
template <>
void VP8Raster::Block<4>::sse_horiz_inter_predict( const uint8_t * src,
                                                   const unsigned int pixels_per_line,
                                                   const uint8_t * dst,
                                                   const unsigned int dst_pitch,
                                                   const unsigned int dst_height,
                                                   const unsigned int filter_idx )
{
  vp8_filter_block1d4_h6_ssse3( src, pixels_per_line, dst, dst_pitch, dst_height,
                                filter_idx );
}

template <>
void VP8Raster::Block<8>::sse_horiz_inter_predict( const uint8_t * src,
                                                   const unsigned int pixels_per_line,
                                                   const uint8_t * dst,
                                                   const unsigned int dst_pitch,
                                                   const unsigned int dst_height,
                                                   const unsigned int filter_idx )
{
  vp8_filter_block1d8_h6_ssse3( src, pixels_per_line, dst, dst_pitch, dst_height,
                                filter_idx );
}

template <>
void VP8Raster::Block<16>::sse_horiz_inter_predict( const uint8_t * src,
                                                    const unsigned int pixels_per_line,
                                                    const uint8_t * dst,
                                                    const unsigned int dst_pitch,
                                                    const unsigned int dst_height,
                                                    const unsigned int filter_idx )
{
  vp8_filter_block1d16_h6_ssse3( src, pixels_per_line, dst, dst_pitch, dst_height,
                                filter_idx );
}

template <>
void VP8Raster::Block<4>::sse_vert_inter_predict( const uint8_t * src,
                                                  const unsigned int pixels_per_line,
                                                  const uint8_t * dst,
                                                  const unsigned int dst_pitch,
                                                  const unsigned int dst_height,
                                                  const unsigned int filter_idx )
{
  vp8_filter_block1d4_v6_ssse3( src, pixels_per_line, dst, dst_pitch, dst_height,
                                filter_idx );
}

template <>
void VP8Raster::Block<8>::sse_vert_inter_predict( const uint8_t * src,
                                                  const unsigned int pixels_per_line,
                                                  const uint8_t * dst,
                                                  const unsigned int dst_pitch,
                                                  const unsigned int dst_height,
                                                  const unsigned int filter_idx )
{
  vp8_filter_block1d8_v6_ssse3( src, pixels_per_line, dst, dst_pitch, dst_height,
                                filter_idx );
}

template <>
void VP8Raster::Block<16>::sse_vert_inter_predict( const uint8_t * src,
                                                   const unsigned int pixels_per_line,
                                                   const uint8_t * dst,
                                                   const unsigned int dst_pitch,
                                                   const unsigned int dst_height,
                                                   const unsigned int filter_idx )
{
  vp8_filter_block1d16_v6_ssse3( src, pixels_per_line, dst, dst_pitch, dst_height,
                                filter_idx );
}

#endif

template <unsigned int size>
void VP8Raster::Block<size>::unsafe_inter_predict( const MotionVector & mv, const TwoD< uint8_t > & reference,
                                                   const int source_column, const int source_row,
                                                   TwoDSubRange<uint8_t, size, size> & output ) const
{
  assert( output.stride() == reference.width() );

  const unsigned int stride = output.stride();

  const uint8_t mx = mv.x() & 7, my = mv.y() & 7;

  if ( (mx & 7) == 0 and (my & 7) == 0 ) {
    uint8_t *dest_row_start = &output.at( 0, 0 );
    const uint8_t *src_row_start = &reference.at( source_column, source_row );
    const uint8_t *dest_last_row_start = dest_row_start + size * output.stride();
    while ( dest_row_start != dest_last_row_start ) {
      memcpy( dest_row_start, src_row_start, size );
      dest_row_start += stride;
      src_row_start += stride;
    }
    return;
  }

#ifdef HAVE_SSE2
  alignas(16) SafeArray< SafeArray< uint8_t, size + 8 >, size + 8 > intermediate{};
  const uint8_t *intermediate_ptr = &intermediate.at( 0 ).at( 0 );
  const uint8_t *src_ptr = &reference.at( source_column, source_row );
  const uint8_t *dst_ptr = &output.at( 0, 0 );

  if ( mx ) {
    if ( my ) {
      sse_horiz_inter_predict( src_ptr - 2 * stride, stride, intermediate_ptr,
                               size, size + 5, mx );
      sse_vert_inter_predict( intermediate_ptr, size, dst_ptr, stride,
                              size, my );
    }
    else {
      /* First pass only */
      sse_horiz_inter_predict( src_ptr, stride, dst_ptr, stride, size, mx );
    }
  }
  else {
    /* Second pass only */
    sse_vert_inter_predict( src_ptr - 2 * stride, stride, dst_ptr, stride,
                            size, my );
  }

#else
  SafeArray< SafeArray< uint8_t, size >, size + 5 > intermediate;

  {
    uint8_t *intermediate_row_start = &intermediate.at( 0 ).at( 0 );
    const uint8_t *intermediate_last_row_start = intermediate_row_start + size * (size + 5);
    const uint8_t *src_row_start = &reference.at( source_column - 2, source_row - 2 );

    const auto & horizontal_filter = sixtap_filters.at( mx );

    while ( intermediate_row_start != intermediate_last_row_start ) {
      const uint8_t *intermediate_row_end = intermediate_row_start + size;

      while ( intermediate_row_start != intermediate_row_end ) {
        *( intermediate_row_start ) =
          clamp255( ( (   *( src_row_start )     * horizontal_filter.at( 0 ) )
                      + ( *( src_row_start + 1 ) * horizontal_filter.at( 1 ) )
                      + ( *( src_row_start + 2 ) * horizontal_filter.at( 2 ) )
                      + ( *( src_row_start + 3 ) * horizontal_filter.at( 3 ) )
                      + ( *( src_row_start + 4 ) * horizontal_filter.at( 4 ) )
                      + ( *( src_row_start + 5 ) * horizontal_filter.at( 5 ) )
                      + 64 ) >> 7 );

        intermediate_row_start++;
        src_row_start++;
      }
      src_row_start += stride - size;
    }
  }

  {
    uint8_t *dest_row_start = &output.at( 0, 0 );
    const uint8_t *dest_last_row_start = dest_row_start + size * stride;
    const uint8_t *intermediate_row_start = &intermediate.at( 0 ).at( 0 );

    const auto & vertical_filter = sixtap_filters.at( my );

    while ( dest_row_start != dest_last_row_start ) {
      const uint8_t *dest_row_end = dest_row_start + size;

      while ( dest_row_start != dest_row_end ) {
        *dest_row_start =
          clamp255( ( (   *( intermediate_row_start )            * vertical_filter.at( 0 ) )
                      + ( *( intermediate_row_start + size )     * vertical_filter.at( 1 ) )
                      + ( *( intermediate_row_start + size * 2 ) * vertical_filter.at( 2 ) )
                      + ( *( intermediate_row_start + size * 3 ) * vertical_filter.at( 3 ) )
                      + ( *( intermediate_row_start + size * 4 ) * vertical_filter.at( 4 ) )
                      + ( *( intermediate_row_start + size * 5 ) * vertical_filter.at( 5 ) )
                      + 64 ) >> 7 );

        dest_row_start++;
        intermediate_row_start++;
      }
      dest_row_start += stride - size;
    }
  }
#endif
}

template <unsigned int size>
template <class ReferenceType>
void VP8Raster::Block<size>::safe_inter_predict( const MotionVector & mv, const ReferenceType & reference,
                                                 const int source_column, const int source_row,
                                                 TwoDSubRange<uint8_t, size, size> & output ) const
{
  if ( (mv.x() & 7) == 0 and (mv.y() & 7) == 0 ) {
    output.forall_ij(
      [&] ( uint8_t & val, unsigned int column, unsigned int row )
      {
        val = reference.at( source_column + column, source_row + row );
      }
    );

    return;
  }

  /* filter horizontally */
  const auto & horizontal_filter = sixtap_filters.at( mv.x() & 7 );

  SafeArray<SafeArray<uint8_t, size>, size + 5> intermediate{};

  for ( uint8_t row = 0; row < size + 5; row++ ) {
    for ( uint8_t column = 0; column < size; column++ ) {
      const int real_row = source_row + row - 2;
      const int real_column = source_column + column;
      intermediate.at( row ).at( column ) =
        clamp255( ( ( reference.at( real_column - 2, real_row ) * horizontal_filter.at( 0 ) )
                  + ( reference.at( real_column - 1, real_row ) * horizontal_filter.at( 1 ) )
                  + ( reference.at( real_column,     real_row ) * horizontal_filter.at( 2 ) )
                  + ( reference.at( real_column + 1, real_row ) * horizontal_filter.at( 3 ) )
                  + ( reference.at( real_column + 2, real_row ) * horizontal_filter.at( 4 ) )
                  + ( reference.at( real_column + 3, real_row ) * horizontal_filter.at( 5 ) )
                  + 64 ) >> 7 );
    }
  }

  /* filter vertically */
  const auto & vertical_filter = sixtap_filters.at( mv.y() & 7 );

  for ( uint8_t row = 0; row < size; row++ ) {
    for ( uint8_t column = 0; column < size; column++ ) {
      output.at( column, row ) =
        clamp255( ( ( intermediate.at( row     ).at( column ) * vertical_filter.at( 0 ) )
                  + ( intermediate.at( row + 1 ).at( column ) * vertical_filter.at( 1 ) )
                  + ( intermediate.at( row + 2 ).at( column ) * vertical_filter.at( 2 ) )
                  + ( intermediate.at( row + 3 ).at( column ) * vertical_filter.at( 3 ) )
                  + ( intermediate.at( row + 4 ).at( column ) * vertical_filter.at( 4 ) )
                  + ( intermediate.at( row + 5 ).at( column ) * vertical_filter.at( 5 ) )
                  + 64 ) >> 7 );
    }
  }
}

template class VP8Raster::Block<4>;
template class VP8Raster::Block<8>;
template class VP8Raster::Block<16>;
