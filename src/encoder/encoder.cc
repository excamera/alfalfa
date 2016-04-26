#include "macroblock.hh"

template <class FrameHeaderType, class MacroblockHeaderType>
SafeArray< int16_t, 16 > Macroblock<FrameHeaderType, MacroblockHeaderType>::extract_y_dc_coeffs( bool set_to_zero ) {
  SafeArray< int16_t, 16 > result;

  for ( size_t column = 0; column < 4; column++ ) {
    for ( size_t row = 0; row < 4; row++ ) {
      result.at( column * 4 + row ) = Y_.at( column, row ).coefficients().at( 0 );

      if ( set_to_zero ) {
        Y_.at( column, row ).set_dc_coefficient( 0 );
      }
    }
  }

  return result;
}
