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

    Player<Decoder> player( argv[ 1 ] );

    VideoDisplay display { player.example_raster() };

    while ( not player.eof() ) {
      display.draw( player.advance() );
    }
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
