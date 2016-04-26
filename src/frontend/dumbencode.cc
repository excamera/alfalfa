#include <fstream>
#include <algorithm>
#include <iostream>
#include <string>

#include "frame.hh"
#include "player.hh"
#include "vp8_raster.hh"
#include "decoder.hh"
#include "encoder.hh"
#include "macroblock.hh"
#include "ivf_writer.hh"

using namespace std;

int main( int argc, char *argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc != 3 ) {
      cerr << "Usage: " << argv[ 0 ] << " <input-video> <output-video>" << endl;
      return EXIT_FAILURE;
    }

    FilePlayer input_player( argv[ 1 ] );
    Encoder encoder( argv[ 2 ], input_player.width(), input_player.height() );

    while ( not input_player.eof() ) {
      encoder.encode_as_keyframe( input_player.advance() );
    }
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
