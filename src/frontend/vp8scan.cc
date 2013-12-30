#include <iostream>

#include "ivf.hh"
#include "decoder.hh"
#include "display.hh"

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

    Decoder decoder( file.width(), file.height() );
    Raster raster( file.width(), file.height() );
    VideoDisplay display( raster );

    for ( uint32_t i = 0; i < file.frame_count(); i++ ) {
      if ( decoder.decode_frame( file.frame( i ), raster ) ) {
	display.draw( raster );
      }
    }
  } catch ( const Exception & e ) {
    e.perror( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
