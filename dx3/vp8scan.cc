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
      printf( "%x %x %x\n", frame.octet( 0 ), frame.octet( 1 ), frame.octet( 2 ) );
    }

  } catch ( const Exception & e ) {
    e.perror();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
