#include "macroblock_header.hh"
#include "raster.hh"

template <unsigned int size>
void Raster::Block<size>::tm_predict( void )
{
  if ( context.above.initialized() and context.left.initialized() ) {
    assert( context.above_left.initialized() );
    const Component P = context.above_left.get()->contents.at( size - 1, size - 1 );
    const auto left = context.left.get()->right();
    const auto above = context.above.get()->bottom();
    contents.forall_ij( [&] ( Component & b, unsigned int column, unsigned int row )
			{ b = left.at( 0, row ) + above.at( column, 0 ) - P; } );
  } else if ( context.above.initialized() ) {
    v_predict();
  } else {
    h_predict();
  }
}

template <unsigned int size>
void Raster::Block<size>::h_predict( void )
{
  if ( context.left.initialized() ) {
    const auto left = context.left.get()->right();
    for ( unsigned int row = 0; row < size; row++ ) {
      contents.row( row ).fill( left.at( 0, row ) );
    }
  } else {
    contents.fill( 129 );
  }
}

template <unsigned int size>
void Raster::Block<size>::v_predict( void )
{
  if ( context.above.initialized() ) {
    const auto above = context.above.get()->bottom();
    for ( unsigned int column = 0; column < size; column++ ) {
      contents.column( column ).fill( above.at( column, 0 ) );
    }
  } else {
    contents.fill( 127 );
  }
}

template <unsigned int size>
void Raster::Block<size>::dc_predict( void )
{
  Component value = 128;

  if ( context.above.initialized() and context.left.initialized() ) {
    value = ((context.above.get()->bottom().sum(int16_t())
	      + context.left.get()->right().sum(int16_t())) + (1 << 3)) >> 4;
  } else if ( context.above.initialized() ) {
    value = (context.above.get()->bottom().sum(int16_t()) + (1 << 2)) >> 3;
  } else if ( context.left.initialized() ) {
    value = (context.left.get()->right().sum(int16_t()) + (1 << 2)) >> 3;
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
  case B_PRED: assert( false ); break; /* should not be possible */
  }
}

void KeyFrameMacroblockHeader::intra_predict( void )
{
  raster_.get()->U.intra_predict( uv_prediction_mode() );
  raster_.get()->V.intra_predict( uv_prediction_mode() );
}
