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

    /* find first key frame */
    uint32_t frame_no = 0;

    while ( frame_no < file.frame_count() ) {
      UncompressedChunk uncompressed_chunk( file.frame( frame_no ), file.width(), file.height() );
      if ( uncompressed_chunk.key_frame() ) {
	break;
      }
      frame_no++;
    }

    Decoder decoder( file.width(), file.height(), file.frame( frame_no ) );
    Raster raster( file.width(), file.height() );

    for ( uint32_t i = frame_no; i < file.frame_count(); i++ ) {
      if ( UncompressedChunk( file.frame( i ), file.width(), file.height() ).key_frame() ) {

	if ( decoder.decode_frame( file.frame( i ), raster ) ) {
	  raster.Y().forall( [&] ( const uint8_t & pixel ) { cout << pixel; } );
	  raster.U().forall( [&] ( const uint8_t & pixel ) { cout << pixel; } );
	  raster.V().forall( [&] ( const uint8_t & pixel ) { cout << pixel; } );
	}
      }
    }
  } catch ( const Exception & e ) {
    e.perror( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
