#include <cmath>

#include "macroblock_header.hh"
#include "raster.hh"

template <unsigned int size>
const typename Raster::Block<size>::Predictors::Row & Raster::Block<size>::Predictors::row127( void )
{
  static TwoD< Component > storage( size, 1, 127 );
  static const Row row( storage, 0, 0 );
  return row;
}

template <unsigned int size>
const typename Raster::Block<size>::Predictors::Column & Raster::Block<size>::Predictors::col129( void )
{
  static TwoD< Component > storage( 1, size, 129 );
  static const Column col( storage, 0, 0 );
  return col;
}

template <unsigned int size>
Raster::Block<size>::Predictors::Predictors( const typename TwoD< Block >::Context & context )
  : above( context.above.initialized()
	   ? context.above.get()->contents.row( size - 1 )
	   : row127() ),
    left( context.left.initialized()
	  ? context.left.get()->contents.column( size - 1 )
	  : col129() ),
    above_left( context.above_left.initialized()
		? &context.above_left.get()->at( size - 1, size - 1 )
		: ( context.above.initialized()
		    ? &col129().at( 0, 0 )
		    : &row127().at( 0, 0 ) ) ),
    above_right_bottom_row( context.above_right.initialized()
			    ? context.above_right.get()->row( size - 1 )
			    : row127() ),
  above_bottom_right_pixel( context.above.initialized()
			    ? &context.above.get()->at( size - 1, size - 1 )
			    : &row127().at( 0, 0 ) )
{}

template <unsigned int size>
const Raster::Component & Raster::Block<size>::Predictors::above_right( const unsigned int column ) const
{
  return context.above_right.initialized() ? above_right_bottom_row.at( column, 0 ) : above_bottom_right_pixel;
}

template <unsigned int size>
void Raster::Block<size>::tm_predict( void )
{
  contents.forall_ij( [&] ( Component & b, unsigned int column, unsigned int row )
		      { b.clamp( predictors.left.at( 0, row )
				 + predictors.above.at( column, 0 )
				 - predictors.above_left ); } );
}

template <unsigned int size>
void Raster::Block<size>::h_predict( void )
{
  for ( unsigned int row = 0; row < size; row++ ) {
    contents.row( row ).fill( predictors.left.at( 0, row ) );
  }
}

template <unsigned int size>
void Raster::Block<size>::v_predict( void )
{
  for ( unsigned int column = 0; column < size; column++ ) {
    contents.column( column ).fill( predictors.above.at( column, 0 ) );
  }
}

template <unsigned int size>
void Raster::Block<size>::dc_predict_simple( void )
{
  static_assert( size == 4 or size == 8 or size == 16, "invalid Block size" );
  static constexpr uint8_t log2size = size == 4 ? 2 : size == 8 ? 3 : size == 16 ? 4 : 0;

  contents.fill( ((predictors.above.sum(int16_t())
		   + predictors.left.sum(int16_t())) + (1 << log2size))
		 >> (log2size+1) );
}

template <unsigned int size>
void Raster::Block<size>::dc_predict( void )
{
  if ( context.above.initialized() and context.left.initialized() ) {
    return dc_predict_simple();
  }

  Component value = 128;
  static_assert( size == 4 or size == 8 or size == 16, "invalid Block size" );
  static constexpr uint8_t log2size = size == 4 ? 2 : size == 8 ? 3 : size == 16 ? 4 : 0;

  if ( context.above.initialized() ) {
    value = (predictors.above.sum(int16_t()) + (1 << (log2size-1))) >> log2size;
  } else if ( context.left.initialized() ) {
    value = (predictors.left.sum(int16_t()) + (1 << (log2size-1))) >> log2size;
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

/*
template <>
template <>
void Raster::Block4::ve_predict( void )
{
  const auto above = above_predictor();
  std::array< uint8_t, 4 > smoothed
    = {{ (above_left_predictor() + 2 * above.at( 0, 0 ) + above.at( 1, 0 ) + 2) >> 2,
	 (above.at( 0, 0 ) + 2 * above.at( 1, 0 ) + above.at( 2, 0 ) + 2) >> 2,
	 (above.at( 1, 0 ) + 2 * above.at( 2, 0 ) + above.at( 3, 0 ) + 2) >> 2,
	 (above.at( 2, 0 ) + 2 * above.at( 3, 0 ) + above.at( 4, 
	 
}
*/

template <>
template <>
void Raster::Block4::intra_predict( const intra_bmode b_mode )
{
  /* Luma prediction */

  switch ( b_mode ) {
  case B_DC_PRED: dc_predict_simple(); break; 
  case B_TM_PRED: tm_predict(); break;
  case B_VE_PRED: 
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
