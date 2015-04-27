#include <iostream>

#include "player.hh"

using namespace std;

int main( int argc, char *argv[] )
{
  try {
    if ( argc != 2 ) {
      cerr << "Usage: " << argv[ 0 ] << " FILENAME" << endl;
      return EXIT_FAILURE;
    }

    Player player( argv[ 1 ] );

    while ( not player.eof() ) {
      RasterHandle raster = player.advance();

      for ( unsigned int row = 0; row < raster.get().display_height(); row++ ) {
	fwrite( &raster.get().Y().at( 0, row ), raster.get().display_width(), 1, stdout );
      }
      for ( unsigned int row = 0; row < (1 + raster.get().display_height()) / 2; row++ ) {
	fwrite( &raster.get().U().at( 0, row ), (1 + raster.get().display_width()) / 2, 1, stdout );
      }
      for ( unsigned int row = 0; row < (1 + raster.get().display_height()) / 2; row++ ) {
	fwrite( &raster.get().V().at( 0, row ), (1 + raster.get().display_width()) / 2, 1, stdout );
      }
    }
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
