/* Based on libvpx vp8_short_fdct4x4_c */

#include "block.hh"

template <BlockType initial_block_type, class PredictionMode>
void Block<initial_block_type, PredictionMode>::fdct( const Raster::Block4 & input )
{
  for ( uint8_t row = 0; row < 4; row++ ) {
    const int16_t a1 = (input.contents().at( 0, row ) + input.contents().at( 3, row )) * 8;
    const int16_t b1 = (input.contents().at( 1, row ) + input.contents().at( 2, row )) * 8;
    const int16_t c1 = (input.contents().at( 1, row ) - input.contents().at( 2, row )) * 8;
    const int16_t d1 = (input.contents().at( 0, row ) - input.contents().at( 3, row )) * 8;

    coefficients_.at( row * 4 + 0 ) = a1 + b1;
    coefficients_.at( row * 4 + 2 ) = a1 - b1;
    coefficients_.at( row * 4 + 1 ) = (c1 * 2217 + d1 * 5352 + 14500) >> 12;
    coefficients_.at( row * 4 + 3 ) = (d1 * 2217 - c1 * 5352 +  7500) >> 12;
  }

  for ( uint8_t i = 0; i < 4; i++ ) {
    const int16_t a1 = coefficients_.at( i + 0 ) + coefficients_.at( i + 12 );
    const int16_t b1 = coefficients_.at( i + 4 ) + coefficients_.at( i + 8 );
    const int16_t c1 = coefficients_.at( i + 4 ) - coefficients_.at( i + 8 );
    const int16_t d1 = coefficients_.at( i + 0 ) - coefficients_.at( i + 12 );

    coefficients_.at( i + 0 ) = (a1 + b1 + 7) >> 4;
    coefficients_.at( i + 8 ) = (a1 - b1 + 7) >> 4;
    coefficients_.at( i + 4 ) = ( (c1 * 2217 + d1 * 5352 + 12000) >> 16 ) + ( d1 != 0 );
    coefficients_.at( i + 12) = (d1 * 2217 - c1 * 5352 + 51000) >> 16;
  }
}
