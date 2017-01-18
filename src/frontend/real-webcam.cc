/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <getopt.h>

#include <iostream>
#include <thread>
#include <chrono>

#include "camera.hh"
#include "display.hh"

using namespace std;

void usage( const char *argv0 )
{
  cerr << "Usage: " << argv0
       << " [-d, --device CAMERA] [-p, --pixfmt PIXEL_FORMAT]"
       << endl;
}

int main( int argc, char *argv[] )
{
  /* check the command-line arguments */
  if ( argc < 1 ) { /* for sticklers */
    abort();
  }

  /* camera settings */
  string camera_device = "/dev/video0";
  string pixel_format = "NV12";

  const option command_line_options[] = {
    { "device", required_argument, nullptr, 'd' },
    { "pixfmt", required_argument, nullptr, 'p' },
    { 0, 0, 0, 0 }
  };

  while ( true ) {
    const int opt = getopt_long( argc, argv, "d:p:", command_line_options, nullptr );

    if ( opt == -1 ) {
      break;
    }

    switch ( opt ) {
    case 'd':
      camera_device = optarg;
      break;

    case 'p':
      pixel_format = optarg;
      break;

    default:
      usage( argv[ 0 ] );
      return EXIT_FAILURE;
    }
  }

  Camera camera { 1280, 720, PIXEL_FORMAT_STRS.at( pixel_format ), camera_device };

  RasterHandle r { MutableRasterHandle{ 1280, 720 } };
  VideoDisplay display { r };

  while ( true ) {
    auto raster = camera.get_next_frame();

    if ( raster.initialized() ) {
      display.draw( raster.get() );
    }
  }

  return EXIT_SUCCESS;
}
