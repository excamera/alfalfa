#include <cmath>

#include "macroblock_header.hh"
#include "raster.hh"

template <unsigned int size>
const typename Raster::Block<size>::Row & Raster::Block<size>::row127( void )
{
  static TwoD< Component > storage( size, 1, 127 );
  static const Row row( storage, 0, 0 );
  return row;
}

template <unsigned int size>
const typename Raster::Block<size>::Column & Raster::Block<size>::col129( void )
{
  static TwoD< Component > storage( 1, size, 129 );
  static const Column col( storage, 0, 0 );
  return col;
}

template <unsigned int size>
const typename Raster::Block<size>::Row Raster::Block<size>::above_predictor( void ) const
{
  return context.above.initialized() ? context.above.get()->contents.row( size - 1 ) : row127();
}

template <unsigned int size>
const typename Raster::Block<size>::Column Raster::Block<size>::left_predictor( void ) const
{
  return context.left.initialized() ? context.left.get()->contents.column( size - 1 ) : col129();
}

template <unsigned int size>
Raster::Component Raster::Block<size>::above_left_predictor( void ) const
{
  if ( context.above_left.initialized() ) {
    return context.above_left.get()->at( size - 1, size - 1 );
  } else if ( context.above.initialized() ) {
    return 129;
  } else {
    return 127;
  }
}

template <unsigned int size>
void Raster::Block<size>::tm_predict( void )
{
  contents.forall_ij( [&] ( Component & b, unsigned int column, unsigned int row )
		      { b.clamp( left_predictor().at( 0, row )
				 + above_predictor().at( column, 0 )
				 - above_left_predictor() ); } );
}

template <unsigned int size>
void Raster::Block<size>::h_predict( void )
{
  for ( unsigned int row = 0; row < size; row++ ) {
    contents.row( row ).fill( left_predictor().at( 0, row ) );
  }
}

template <unsigned int size>
void Raster::Block<size>::v_predict( void )
{
  for ( unsigned int column = 0; column < size; column++ ) {
    contents.column( column ).fill( above_predictor().at( column, 0 ) );
  }
}

template <unsigned int size>
void Raster::Block<size>::dc_predict( void )
{
  Component value = 128;
  static_assert( size == 4 or size == 8 or size == 16, "invalid Block size" );
  static constexpr uint8_t log2size = size == 4 ? 2 : size == 8 ? 3 : size == 16 ? 4 : 0;

  if ( context.above.initialized() and context.left.initialized() ) {
    value = ((above_predictor().sum(int16_t()) + left_predictor().sum(int16_t())) + (1 << log2size))
      >> (log2size+1);
  } else if ( context.above.initialized() ) {
    value = (above_predictor().sum(int16_t()) + (1 << (log2size-1))) >> log2size;
  } else if ( context.left.initialized() ) {
    value = (left_predictor().sum(int16_t()) + (1 << (log2size-1))) >> log2size;
  }

  contents.fill( value );
}

template <>
template <>
void Raster::Block8::intra_predict( const intra_mbmode uv_mode )
{
  /* Chroma prediction */

  switch ( uv_mode ) {
  case DC_PRED: dc_predict(); break;
  case V_PRED:  v_predict();  break;
  case H_PRED:  h_predict();  break;
  case TM_PRED: tm_predict(); break;
  case B_PRED: assert( false ); break; /* tree decoder for uv_mode can't produce this */
  }
}

template <>
template <>
void Raster::Block4::intra_predict( const intra_bmode b_mode )
{
  /* Luma prediction */

  switch ( b_mode ) {
  case B_DC_PRED: dc_predict(); break; 
  case B_TM_PRED: tm_predict(); break;
  case B_VE_PRED: break;
  case B_HE_PRED:
  case B_LD_PRED:
  case B_RD_PRED:
  case B_VR_PRED:
  case B_VL_PRED:
  case B_HD_PRED:
  case B_HU_PRED: break;
  }
}

void KeyFrameMacroblockHeader::intra_predict( void )
{
  /* Chroma prediction */
  raster_.get()->U.intra_predict( uv_prediction_mode() );
  raster_.get()->V.intra_predict( uv_prediction_mode() );

  /* Luma prediction */
  switch ( Y2_.prediction_mode() ) {
  case DC_PRED: raster_.get()->Y.dc_predict(); break;
  case V_PRED:  raster_.get()->Y.v_predict();  break;
  case H_PRED:  raster_.get()->Y.h_predict();  break;
  case TM_PRED: raster_.get()->Y.tm_predict(); break;    
  case B_PRED:
    raster_.get()->Y_sub.forall_ij( [&] ( Raster::Block4 & block, unsigned int column, unsigned int row )
				    { block.intra_predict( Y_.at( column, row ).prediction_mode() ); } );
    break;
  }
}
