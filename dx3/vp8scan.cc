#include <iostream>

#include "ivf.hh"

using namespace std;

int main( int argc, char *argv[] )
{
  try {
    if ( argc != 2 ) {
      cerr << "Usage: " << argv[ 0 ] << " FILENAME" << endl;
      return EXIT_FAILURE;
    }

    IVF file( argv[ 1 ] );

    cout << "fourcc: " << file.fourcc() << endl;
    cout << "width: " << file.width() << endl;
    cout << "height: " << file.height() << endl;
    cout << "frame_rate: " << file.frame_rate() << endl;
    cout << "time_scale: " << file.time_scale() << endl;
    cout << "frame_count: " << file.frame_count() << endl;

    for ( uint32_t i = 0; i < file.frame_count(); i++ ) {
      Block frame = file.frame( i );
      Block tag = frame( 0, 3 );

      const uint32_t size = frame.size();
      const bool interframe = tag.bits( 0, 1 );
      const uint8_t version = tag.bits( 1, 3 );
      const bool show_frame = tag.bits( 4, 1 );
      const uint32_t first_partition_length = tag.bits( 5, 19 );

      printf( "frame %u: size: %u, interframe: %u, version: %u, show_frame: %u, size0: %u\n",
	      i + 1, size, interframe, version, show_frame, first_partition_length );

      if ( not interframe ) {
	if ( frame( 3, 3 ).to_string() != "\x9d\x01\x2a" ) {
	  throw Exception( argv[ 1 ], "invalid key frame header" );
	}

	Block sizes = frame( 6, 4 );

	const char horizontal_scale = sizes.bits( 14, 2 ), vertical_scale = sizes.bits( 30, 2 );
	const uint16_t width = sizes.bits( 0, 14 ), height = sizes.bits( 16, 14 );

	printf( "hscale = %d, vscale = %d, width = %u, height = %u\n",
		horizontal_scale, vertical_scale, width, height );
      }
   }

  } catch ( const Exception & e ) {
    e.perror();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
