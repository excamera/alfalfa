/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <iostream>

#include "camera.hh"
#include "display.hh"
#include <thread>
#include <chrono>

using namespace std;

void usage( const char *argv0 )
{
  cerr << "Usage: " << argv0 << endl;
}

int main( int argc, char *argv[] )
{
  /* check the command-line arguments */
  if ( argc < 1 ) { /* for sticklers */
    abort();
  }

  if ( argc != 1 ) {
    usage( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  RasterHandle r { MutableRasterHandle{ 1280, 720 } };

  VideoDisplay display { r };
  Camera camera { 1280, 720 };

  while ( true ) {
    display.draw( camera.get_frame() );
  }

  return EXIT_SUCCESS;
}
