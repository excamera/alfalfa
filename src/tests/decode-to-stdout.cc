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

    for ( uint32_t i = frame_no; i < file.frame_count(); i++ ) {
      RasterHandle raster_handle( file.width(), file.height() );

      if ( decoder.decode_frame( file.frame( i ), raster_handle ) ) {
	const Raster & raster = raster_handle;

	for ( unsigned int row = 0; row < raster.display_height(); row++ ) {
	  fwrite( &raster.Y().at( 0, row ), raster.display_width(), 1, stdout );
	}
	for ( unsigned int row = 0; row < (1 + raster.display_height()) / 2; row++ ) {
	  fwrite( &raster.U().at( 0, row ), (1 + raster.display_width()) / 2, 1, stdout );
	}
	for ( unsigned int row = 0; row < (1 + raster.display_height()) / 2; row++ ) {
	  fwrite( &raster.V().at( 0, row ), (1 + raster.display_width()) / 2, 1, stdout );
	}
      }
    }
  } catch ( const Exception & e ) {
    e.perror( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
