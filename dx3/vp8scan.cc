#include <iostream>

#include "file.hh"

using namespace std;

int main( int argc, char *argv[] )
{
  if ( argc != 2 ) {
    cerr << "Usage: " << argv[ 0 ] << " FILENAME" << endl;
    return EXIT_FAILURE;
  }

  try {
    File file( argv[ 1 ] );

    for ( uint64_t i = 0; i < file.size(); i++ ) {
      cout << i << ": " << file[ i ] << endl;
    }
  } catch ( const Exception & e ) {
    e.perror();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
