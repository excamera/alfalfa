#include <cstdlib>
#include <iostream>

using namespace std;

void usage( const char *argv0 )
{
  cerr << "Usage: " << argv0 << " INPUT HOST PORT" << endl;
}

int main( int argc, char *argv[] )
{
  /* check the command-line arguments */
  if ( argc < 1 ) { /* for sticklers */
    abort();
  }

  if ( argc != 4 ) {
    usage( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
