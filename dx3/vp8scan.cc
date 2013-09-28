#include <iostream>

#include "ivf.hh"
#include "vp8_decoder.hh"

using namespace std;

int main( int argc, char *argv[] )
{
  try {
    if ( argc != 2 ) {
      cerr << "Usage: " << argv[ 0 ] << " FILENAME" << endl;
      return EXIT_FAILURE;
    }

    IVF file( argv[ 1 ] );

    if ( file.fourcc() != "VP80" ) {
      throw Exception( argv[ 1 ], "not a VP80 file" );
    }

    VP8Decoder vp8( file.width(), file.height() );

    for ( uint32_t i = 0; i < file.frame_count(); i++ ) {
      vp8.decode_frame( file.frame( i ) );
    }

  } catch ( const Exception & e ) {
    e.perror( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
