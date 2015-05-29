#include <iostream>

#include "player.hh"

using namespace std;

int main( int argc, char * argv[] )
{
  try {
    if ( argc < 4 ) {
      cerr << "Usage: " << argv[ 0 ] << " SWITCH_NO SOURCE TARGET " << endl;
      return EXIT_FAILURE;
    }

    unsigned int switch_frame = stoi( argv[ 1 ] );

    Player source_player( argv[ 2 ] );
    Player target_player( argv[ 3 ] );

    while ( source_player.cur_displayed_frame() < switch_frame ) {
      RasterHandle output = source_player.advance();
      output.get().dump( stdout );
    }

    while ( target_player.cur_displayed_frame() < switch_frame - 1 ) {
      target_player.advance();
    }

    while ( not target_player.eof() ) {
      RasterHandle output = target_player.advance();
      output.get().dump( stdout );
    }

  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }
}
