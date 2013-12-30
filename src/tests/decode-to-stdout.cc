#include <iostream>

#include "ivf.hh"
#include "uncompressed_chunk.hh"
#include "decoder.hh"

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
    Raster raster( decoder.raster_width() / 16, decoder.raster_height() / 16,
		   file.width(), file.height() );

    for ( uint32_t i = 0; i < file.frame_count(); i++ ) {
      if ( UncompressedChunk( file.frame( i ), file.width(), file.height() ).key_frame() ) {
	decoder.decode_frame( file.frame( i ), raster );
	raster.Y().forall( [&] ( const uint8_t & pixel ) { cout << pixel; } );
	raster.U().forall( [&] ( const uint8_t & pixel ) { cout << pixel; } );
	raster.V().forall( [&] ( const uint8_t & pixel ) { cout << pixel; } );
      }
    }
  } catch ( const Exception & e ) {
    e.perror( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
