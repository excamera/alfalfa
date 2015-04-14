#include <iostream>

#include "player.hh"
#include "display.hh"

using namespace std;

int main( int argc, char *argv[] )
{
  try {
    if ( argc != 2 ) {
      cerr << "Usage: " << argv[ 0 ] << " FILENAME" << endl;
      return EXIT_FAILURE;
    }

    Player player( argv[ 1 ] );


    VideoDisplay display( player.new_raster() );


    while ( true ) {
      RasterHandle raster = player.new_raster();
      if ( player.next_shown_frame( raster ) ) {
	display.draw( raster );
      }
      else {
	break;
      }
    }
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
