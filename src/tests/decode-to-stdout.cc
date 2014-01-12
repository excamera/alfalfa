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
      if ( decoder.decode_frame( file.frame( i ), raster ) ) {
	unsigned int num_written =
	  fwrite( &raster.Y().at( 0, 0 ), raster.Y().width() * raster.Y().height(), 1, stdout )
	  + fwrite( &raster.U().at( 0, 0 ), raster.U().width() * raster.U().height(), 1, stdout )
	  + fwrite( &raster.V().at( 0, 0 ), raster.V().width() * raster.V().height(), 1, stdout );

	if ( num_written != 3 ) {
	  throw Exception( "fwrite", "short write" );
	}
      }
    }
  } catch ( const Exception & e ) {
    e.perror( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
