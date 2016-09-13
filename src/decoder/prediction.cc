/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <cmath>
#include <algorithm>

#include "macroblock.hh"
#include "vp8_raster.hh"

using namespace std;

template <unsigned int size>
VP8Raster::Block<size>::Block( const unsigned int column, const unsigned int row,
                               TwoD< uint8_t > & raster_component )
  : contents_( raster_component, size * column, size * row ),
    column_( column ),
    row_( row ),
    predictors_( raster_component, column, row )
{}

/* the rightmost Y-subblocks in a macroblock (other than the upper-right subblock) are special-cased */
template <>
void VP8Raster::Block4::set_above_right_bottom_row_predictor( const typename Predictors::AboveRightBottomRowPredictor & replacement )
{
  predictors_.above_right_bottom_row_predictor.above_right_bottom_row.set( replacement.above_right_bottom_row );
  predictors_.above_right_bottom_row_predictor.above_bottom_right_pixel = replacement.above_bottom_right_pixel;
  predictors_.above_right_bottom_row_predictor.use_row = replacement.use_row;
}

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
{
  /* adjust "extra pixels" for rightmost Y subblocks in macroblock (other than the top one) */
  for ( unsigned int row = 1; row < 4; row++ ) {
    Y_sub_at( 3, row ).set_above_right_bottom_row_predictor( Y_sub_at( 3, 0 ).predictors().above_right_bottom_row_predictor );
  }
}

VP8Raster::VP8Raster( const unsigned int display_width, const unsigned int display_height )
: BaseRaster( display_width, display_height,
              16 * macroblock_dimension( display_width ), 16 * macroblock_dimension( display_height ) )
{}

template <unsigned int size>
const typename VP8Raster::Block<size>::Row & VP8Raster::Block<size>::Predictors::row127( void )
{
  static TwoD< uint8_t > storage( size, 1, 127 );
  static const Row row( storage, 0, 0 );
  return row;
}

template <unsigned int size>
const typename VP8Raster::Block<size>::Column & VP8Raster::Block<size>::Predictors::col129( void )
{
  static TwoD< uint8_t > storage( 1, size, 129 );
  static const Column col( storage, 0, 0 );
  return col;
}

template <unsigned int size>
VP8Raster::Block<size>::Predictors::Predictors( TwoD<uint8_t> & component,
                                                const unsigned int column,
                                                const unsigned int row )
  : above_row( row
               ? component.slice<size, 1>( size * column, size * row - 1 )
               : row127() ),
  left_column( column
               ? component.slice<1, size>( size * column - 1, size * row )
               : col129() ),
  above_left( column and row
              ? component.at( size * column - 1, size * row - 1 )
              : ( row
                  ? col129().at( 0, 0 )
                  : row127().at( 0, 0 ) ) ),
  above_right_bottom_row_predictor( {
      ((row and (size * (column + 1) < component.width()))
       ? component.slice<size, 1>( size * (column + 1), size * row - 1 )
       : row127()),
        (row
         ? &above_row.at( size - 1, 0 )
         : &row127().at( 0, 0 )),
        (row and (size * (column + 1) < component.width())) } )
{}

template <unsigned int size>
uint8_t VP8Raster::Block<size>::Predictors::AboveRightBottomRowPredictor::above_right( const unsigned int column ) const
{
  return use_row ? above_right_bottom_row.at( column, 0 ) : *above_bottom_right_pixel;
}

template <unsigned int size>
uint8_t VP8Raster::Block<size>::Predictors::above( const int8_t column ) const
{
  assert( column >= -1 and column < int8_t( size * 2 ) );
  if ( column == -1 ) return above_left;
  if ( 0 <= column and column < int( size ) ) return above_row.at( column, 0 );
  return above_right_bottom_row_predictor.above_right( column - size );
}

template <unsigned int size>
uint8_t VP8Raster::Block<size>::Predictors::left( const int8_t row ) const
{
  assert( row >= -1 and row < int8_t( size ) );
  if ( row == -1 ) return above_left;
  return left_column.at( 0, row );
}

template <unsigned int size>
uint8_t VP8Raster::Block<size>::Predictors::east( const int8_t num ) const
{
  assert( 0 <= num and num <= int8_t( size * 2 ) );
  if ( num <= 4 ) { return left( 3 - num ); }
  return above( num - 5 );
}

template <unsigned int size>
void VP8Raster::Block<size>::true_motion_predict( TwoDSubRange< uint8_t, size, size > & output ) const
{

  output.forall_ij( [&] ( uint8_t & b, unsigned int column, unsigned int row )
                    {
                      b = clamp255( predictors().left_column.at( 0, row )
                                    + predictors().above_row.at( column, 0 )
                                    - predictors().above_left );
                    } );
}

template <unsigned int size>
void VP8Raster::Block<size>::horizontal_predict( TwoDSubRange< uint8_t, size, size > & output ) const
{
  for ( unsigned int row = 0; row < size; row++ ) {
    output.row( row ).fill( predictors().left_column.at( 0, row ) );
  }
}

template <unsigned int size>
void VP8Raster::Block<size>::vertical_predict( TwoDSubRange< uint8_t, size, size > & output ) const
{
  for ( unsigned int column = 0; column < size; column++ ) {
    output.column( column ).fill( predictors().above_row.at( column, 0 ) );
  }
}

template <unsigned int size>
void VP8Raster::Block<size>::dc_predict_simple( TwoDSubRange< uint8_t, size, size > & output ) const
{
  static_assert( size == 4 or size == 8 or size == 16, "invalid Block size" );
  static constexpr uint8_t log2size = size == 4 ? 2 : size == 8 ? 3 : size == 16 ? 4 : 0;

  uint8_t value = ((predictors().above_row.sum(int16_t())
                    + predictors().left_column.sum(int16_t())) + (1 << log2size))
    >> (log2size+1);

  output.fill( value );
}

template <unsigned int size>
void VP8Raster::Block<size>::dc_predict( TwoDSubRange< uint8_t, size, size > & output ) const
{
  if ( column_ and row_ ) {
    return dc_predict_simple( output );
  }

  uint8_t value = 128;
  static_assert( size == 4 or size == 8 or size == 16, "invalid Block size" );
  static constexpr uint8_t log2size = size == 4 ? 2 : size == 8 ? 3 : size == 16 ? 4 : 0;

  if ( row_ > 0 ) {
    value = (predictors().above_row.sum(int16_t()) + (1 << (log2size-1))) >> log2size;
  } else if ( column_ > 0 ) {
    value = (predictors().left_column.sum(int16_t()) + (1 << (log2size-1))) >> log2size;
  }

  output.fill( value );
}

template <>
template <>
void VP8Raster::Block8::intra_predict( const mbmode uv_mode, TwoDSubRange< uint8_t, 8, 8 > & output ) const
{
  /* Chroma prediction */

  switch ( uv_mode ) {
  case DC_PRED: dc_predict( output ); break;
  case V_PRED:  vertical_predict( output );  break;
  case H_PRED:  horizontal_predict( output );  break;
  case TM_PRED: true_motion_predict( output ); break;
  default: throw LogicError(); /* tree decoder for uv_mode can't produce this */
  }
}

template <>
template <>
void VP8Raster::Block16::intra_predict( const mbmode uv_mode, TwoDSubRange< uint8_t, 16, 16 > & output ) const
{
  /* Y prediction for whole macroblock */

  switch ( uv_mode ) {
  case DC_PRED: dc_predict( output ); break;
  case V_PRED:  vertical_predict( output );  break;
  case H_PRED:  horizontal_predict( output );  break;
  case TM_PRED: true_motion_predict( output ); break;
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
void VP8Raster::Block4::vertical_smoothed_predict( TwoDSubRange< uint8_t, 4, 4 > & output ) const
{
  output.column( 0 ).fill( avg3( above( -1 ), above( 0 ), above( 1 ) ) );
  output.column( 1 ).fill( avg3( above( 0 ),  above( 1 ), above( 2 ) ) );
  output.column( 2 ).fill( avg3( above( 1 ),  above( 2 ), above( 3 ) ) );
  output.column( 3 ).fill( avg3( above( 2 ),  above( 3 ), above( 4 ) ) );
}

template <>
void VP8Raster::Block4::horizontal_smoothed_predict( TwoDSubRange< uint8_t, 4, 4 > & output ) const
{
  output.row( 0 ).fill( avg3( left( -1 ), left( 0 ), left( 1 ) ) );
  output.row( 1 ).fill( avg3( left( 0 ),  left( 1 ), left( 2 ) ) );
  output.row( 2 ).fill( avg3( left( 1 ),  left( 2 ), left( 3 ) ) );
  output.row( 3 ).fill( avg3( left( 2 ),  left( 3 ), left( 3 ) ) );
  /* last line is special because we can't use left( 4 ) yet */
}

template <>
void VP8Raster::Block4::left_down_predict( TwoDSubRange< uint8_t, 4, 4 > & output ) const
{
  output.at( 0, 0 ) =                                                             avg3( above( 0 ), above( 1 ), above( 2 ) );
  output.at( 1, 0 ) = output.at( 0, 1 ) =                                         avg3( above( 1 ), above( 2 ), above( 3 ) );
  output.at( 2, 0 ) = output.at( 1, 1 ) = output.at( 0, 2 ) =                     avg3( above( 2 ), above( 3 ), above( 4 ) );
  output.at( 3, 0 ) = output.at( 2, 1 ) = output.at( 1, 2 ) = output.at( 0, 3 ) = avg3( above( 3 ), above( 4 ), above( 5 ) );
  output.at( 3, 1 ) = output.at( 2, 2 ) = output.at( 1, 3 ) =                     avg3( above( 4 ), above( 5 ), above( 6 ) );
  output.at( 3, 2 ) = output.at( 2, 3 ) =                                         avg3( above( 5 ), above( 6 ), above( 7 ) );
  output.at( 3, 3 ) =                                                             avg3( above( 6 ), above( 7 ), above( 7 ) );
  /* last line is special because we don't use above( 8 ) */
}

template <>
void VP8Raster::Block4::right_down_predict( TwoDSubRange< uint8_t, 4, 4 > & output ) const
{
  output.at( 0, 3 ) =                                                             avg3( east( 0 ), east( 1 ), east( 2 ) );
  output.at( 1, 3 ) = output.at( 0, 2 ) =                                         avg3( east( 1 ), east( 2 ), east( 3 ) );
  output.at( 2, 3 ) = output.at( 1, 2 ) = output.at( 0, 1 ) =                     avg3( east( 2 ), east( 3 ), east( 4 ) );
  output.at( 3, 3 ) = output.at( 2, 2 ) = output.at( 1, 1 ) = output.at( 0, 0 ) = avg3( east( 3 ), east( 4 ), east( 5 ) );
  output.at( 3, 2 ) = output.at( 2, 1 ) = output.at( 1, 0 ) =                     avg3( east( 4 ), east( 5 ), east( 6 ) );
  output.at( 3, 1 ) = output.at( 2, 0 ) =                                         avg3( east( 5 ), east( 6 ), east( 7 ) );
  output.at( 3, 0 ) =                                                             avg3( east( 6 ), east( 7 ), east( 8 ) );
}

template <>
void VP8Raster::Block4::vertical_right_predict( TwoDSubRange< uint8_t, 4, 4 > & output ) const
{
  output.at( 0, 3 ) =                     avg3( east( 1 ), east( 2 ), east( 3 ) );
  output.at( 0, 2 ) =                     avg3( east( 2 ), east( 3 ), east( 4 ) );
  output.at( 1, 3 ) = output.at( 0, 1 ) = avg3( east( 3 ), east( 4 ), east( 5 ) );
  output.at( 1, 2 ) = output.at( 0, 0 ) = avg2( east( 4 ), east( 5 ) );
  output.at( 2, 3 ) = output.at( 1, 1 ) = avg3( east( 4 ), east( 5 ), east( 6 ) );
  output.at( 2, 2 ) = output.at( 1, 0 ) = avg2( east( 5 ), east( 6 ) );
  output.at( 3, 3 ) = output.at( 2, 1 ) = avg3( east( 5 ), east( 6 ), east( 7 ) );
  output.at( 3, 2 ) = output.at( 2, 0 ) = avg2( east( 6 ), east( 7 ) );
  output.at( 3, 1 ) =                     avg3( east( 6 ), east( 7 ), east( 8 ) );
  output.at( 3, 0 ) =                     avg2( east( 7 ), east( 8 ) );
}

template <>
void VP8Raster::Block4::vertical_left_predict( TwoDSubRange< uint8_t, 4, 4 > & output ) const
{
  output.at( 0, 0 ) =                     avg2( above( 0 ), above( 1 ) );
  output.at( 0, 1 ) =                     avg3( above( 0 ), above( 1 ), above( 2 ) );
  output.at( 0, 2 ) = output.at( 1, 0 ) = avg2( above( 1 ), above( 2 ) );
  output.at( 1, 1 ) = output.at( 0, 3 ) = avg3( above( 1 ), above( 2 ), above( 3 ) );
  output.at( 1, 2 ) = output.at( 2, 0 ) = avg2( above( 2 ), above( 3 ) );
  output.at( 1, 3 ) = output.at( 2, 1 ) = avg3( above( 2 ), above( 3 ), above( 4 ) );
  output.at( 2, 2 ) = output.at( 3, 0 ) = avg2( above( 3 ), above( 4 ) );
  output.at( 2, 3 ) = output.at( 3, 1 ) = avg3( above( 3 ), above( 4 ), above( 5 ) );
  output.at( 3, 2 ) =                     avg3( above( 4 ), above( 5 ), above( 6 ) );
  output.at( 3, 3 ) =                     avg3( above( 5 ), above( 6 ), above( 7 ) );
}

template <>
void VP8Raster::Block4::horizontal_down_predict( TwoDSubRange< uint8_t, 4, 4 > & output ) const
{
  output.at( 0, 3 ) =                     avg2( east( 0 ), east( 1 ) );
  output.at( 1, 3 ) =                     avg3( east( 0 ), east( 1 ), east( 2 ) );
  output.at( 0, 2 ) = output.at( 2, 3 ) = avg2( east( 1 ), east( 2 ) );
  output.at( 1, 2 ) = output.at( 3, 3 ) = avg3( east( 1 ), east( 2 ), east( 3 ) );
  output.at( 2, 2 ) = output.at( 0, 1 ) = avg2( east( 2 ), east( 3 ) );
  output.at( 3, 2 ) = output.at( 1, 1 ) = avg3( east( 2 ), east( 3 ), east( 4 ) );
  output.at( 2, 1 ) = output.at( 0, 0 ) = avg2( east( 3 ), east( 4 ) );
  output.at( 3, 1 ) = output.at( 1, 0 ) = avg3( east( 3 ), east( 4 ), east( 5 ) );
  output.at( 2, 0 ) =                     avg3( east( 4 ), east( 5 ), east( 6 ) );
  output.at( 3, 0 ) =                     avg3( east( 5 ), east( 6 ), east( 7 ) );
}

template <>
void VP8Raster::Block4::horizontal_up_predict( TwoDSubRange< uint8_t, 4, 4 > & output ) const
{
  output.at( 0, 0 ) =                     avg2( left( 0 ), left( 1 ) );
  output.at( 1, 0 ) =                     avg3( left( 0 ), left( 1 ), left( 2 ) );
  output.at( 2, 0 ) = output.at( 0, 1 ) = avg2( left( 1 ), left( 2 ) );
  output.at( 3, 0 ) = output.at( 1, 1 ) = avg3( left( 1 ), left( 2 ), left( 3 ) );
  output.at( 2, 1 ) = output.at( 0, 2 ) = avg2( left( 2 ), left( 3 ) );
  output.at( 3, 1 ) = output.at( 1, 2 ) = avg3( left( 2 ), left( 3 ), left( 3 ) );
  output.at( 2, 2 ) = output.at( 3, 2 )
                    = output.at( 0, 3 )
                    = output.at( 1, 3 )
                    = output.at( 2, 3 )
                    = output.at( 3, 3 ) = left( 3 );
}

template <>
template <>
void VP8Raster::Block4::intra_predict( const bmode b_mode, TwoDSubRange< uint8_t, 4, 4 > & output ) const
{
  /* Luma prediction */

  switch ( b_mode ) {
  case B_DC_PRED: dc_predict_simple( output ); break;
  case B_TM_PRED: true_motion_predict( output ); break;
  case B_VE_PRED: vertical_smoothed_predict( output ); break;
  case B_HE_PRED: horizontal_smoothed_predict( output ); break;
  case B_LD_PRED: left_down_predict( output ); break;
  case B_RD_PRED: right_down_predict( output ); break;
  case B_VR_PRED: vertical_right_predict( output ); break;
  case B_VL_PRED: vertical_left_predict( output ); break;
  case B_HD_PRED: horizontal_down_predict( output ); break;
  case B_HU_PRED: horizontal_up_predict( output ); break;
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
  alignas(16) SafeArray< SafeArray< uint8_t, size + 8 >, size + 8 > intermediate;
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

  SafeArray<SafeArray<uint8_t, size>, size + 5> intermediate;

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
