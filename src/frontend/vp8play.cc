/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <iostream>
#include <unistd.h>

#include "player.hh"
#include "display.hh"
#include "enc_state_serializer.hh"

using namespace std;

int main( int argc, char *argv[] )
{
  try {
    if ( argc < 2 ) {
      cerr << "Usage: " << argv[ 0 ] << " FILENAME [decoder_state]" << endl;
      return EXIT_FAILURE;
    }

    Player player = argc < 3
      ? Player( argv[ 1 ] )
      : EncoderStateDeserializer::build<Player>(argv[2], argv[1]);

    VideoDisplay display { player.example_raster() };

    while ( not player.eof() ) {
      display.draw( player.advance() );
      cerr << "Displaying frame #" << player.cur_frame_no() << "...";
      getchar();
    }
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
