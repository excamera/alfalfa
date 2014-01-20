#include <iostream>
#include <random>

#include "exception.hh"
#include "block.hh"
#include "raster.hh"

#include "continuation.cc"

using namespace std;

int main( int argc, char *argv[] )
{
  try {
    if ( argc != 1 ) {
      cerr << "Usage: " << argv[ 0 ] << endl;
      return EXIT_FAILURE;
    }

    TwoD< YBlock > block( 1, 1 );
    Raster raster( 16, 16 );

    block.at( 0, 0 ).set_Y_without_Y2();

    random_device rd;
    default_random_engine gen( rd() );
    uniform_int_distribution< Probability > probs( 0, 255 );

    while ( 1 ) {
      SafeArray< SafeArray< uint8_t, 4 >, 4 > random_pixels;

      for ( uint8_t i = 0; i < 4; i++ ) {
	for ( uint8_t j = 0; j < 4; j++ ) {
	  random_pixels.at( i ).at( j ) = probs( gen );
	  raster.macroblock( 0, 0 ).Y_sub.at( 0, 0 ).at( i, j ) = random_pixels.at( i ).at( j );
	}
      }

      Raster zeros( 16, 16 );

      block.at( 0, 0 ).fdct( raster.macroblock( 0, 0 ).Y_sub.at( 0, 0 ) - zeros.macroblock( 0, 0 ).Y_sub.at( 0, 0 ) );

      for ( uint8_t i = 0; i < 4; i++ ) {
	for ( uint8_t j = 0; j < 4; j++ ) {
	  raster.macroblock( 0, 0 ).Y_sub.at( 0, 0 ).at( i, j ) = 0;
	}
      }

      block.at( 0, 0 ).idct_add( raster.macroblock( 0, 0 ).Y_sub.at( 0, 0 ) );

      for ( uint8_t i = 0; i < 4; i++ ) {
	for ( uint8_t j = 0; j < 4; j++ ) {
	  int this_mismatch = raster.macroblock( 0, 0 ).Y_sub.at( 0, 0 ).at( i, j )
	    - random_pixels.at( i ).at( j );
	  printf( "this mismatch: %d\n", this_mismatch );
	}
      }
    }

  } catch ( const Exception & e ) {
    e.perror( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
