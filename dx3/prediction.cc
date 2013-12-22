#include "macroblock_header.hh"
#include "raster.hh"

template <unsigned int size>
void Raster::Block<size>::tm_predict( void )
{
  
}

template <unsigned int size>
void Raster::Block<size>::h_predict( void )
{
  if ( context.left.initialized() ) {
    for ( unsigned int row = 0; row < size; row++ ) {
      const uint8_t value = context.left.get()->contents.at( size - 1, row );
      for ( unsigned int column = 0; column < size; column++ ) {
	contents.at( column, row ) = value;
      }
    }
  } else {
    contents.forall( [&] ( Component & component ) { component = 129; } );    
  }
}

template <unsigned int size>
void Raster::Block<size>::v_predict( void )
{
  if ( context.above.initialized() ) {
    for ( unsigned int column = 0; column < size; column++ ) {
      const uint8_t value = context.above.get()->contents.at( column, size - 1 );
      for ( unsigned int row = 0; row < size; row++ ) {
	contents.at( column, row ) = value;
      }
    }
  } else {
    contents.forall( [&] ( Component & component ) { component = 127; } );    
  }
}

template <unsigned int size>
int16_t Raster::Block<size>::bottom_sum( void ) const
{
  int16_t value = 0;
  for ( unsigned int column = 0; column < size; column++ ) {
    value += contents.at( column, size - 1 );
  }
  return value;
}

template <unsigned int size>
int16_t Raster::Block<size>::right_sum( void ) const
{
  int16_t value = 0;
  for ( unsigned int row = 0; row < size; row++ ) {
    value += contents.at( size - 1, row );
  }
  return value;
}

template <unsigned int size>
void Raster::Block<size>::dc_predict( void )
{
  int16_t value = 128;

  if ( context.above.initialized() and context.left.initialized() ) {
    const int16_t sum = context.above.get()->bottom_sum() + context.left.get()->right_sum();
    value = (sum + (1 << 3)) >> 4;
  } else if ( context.above.initialized() ) {
    const int16_t sum = context.above.get()->bottom_sum();
    value = (sum + (1 << 2)) >> 3;
  } else if ( context.left.initialized() ) {
    const int16_t sum = context.left.get()->right_sum();
    value = (sum + (1 << 2)) >> 3;
  }

  contents.forall( [&] ( Component & component ) { component = value; } );
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
  case B_PRED: assert( false ); break; /* should not be possible */
  }
}

void KeyFrameMacroblockHeader::intra_predict( void )
{
  raster_.get()->U.intra_predict( uv_prediction_mode() );
  raster_.get()->V.intra_predict( uv_prediction_mode() );
}
