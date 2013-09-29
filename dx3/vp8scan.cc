#include <iostream>

#include "ivf.hh"
#include "vp8_parser.hh"

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
      throw Unsupported( "not a VP8 file" );
    }

    VP8Parser vp8( file.width(), file.height() );

    for ( uint32_t i = 0; i < file.frame_count(); i++ ) {
      vp8.parse_frame( file.frame( i ) );
    }
  } catch ( const Exception & e ) {
    e.perror( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
