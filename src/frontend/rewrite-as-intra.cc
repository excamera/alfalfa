#include <iostream>

#include "ivf.hh"
#include "uncompressed_chunk.hh"
#include "decoder_state.hh"

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

    DecoderState decoder_state( file.width(), file.height() );
    References references( file.width(), file.height() );

    for ( uint32_t i = frame_no; i < file.frame_count(); i++ ) {
      RasterHandle raster( file.width(), file.height() );

      UncompressedChunk whole_frame( file.frame( i ), file.width(), file.height() );
      if ( whole_frame.key_frame() ) {
	const KeyFrame parsed_frame = decoder_state.parse_and_apply<KeyFrame>( whole_frame );
	parsed_frame.decode( decoder_state.quantizer_filter_adjustments, raster );
	parsed_frame.copy_to( raster, references );
      } else {
	InterFrame parsed_frame = decoder_state.parse_and_apply<InterFrame>( whole_frame );
	parsed_frame.rewrite_as_intra( decoder_state.quantizer_filter_adjustments, references, raster );
	parsed_frame.copy_to( raster, references );
      }
    }
  } catch ( const Exception & e ) {
    e.perror( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
