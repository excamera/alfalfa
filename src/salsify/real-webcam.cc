/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Copyright 2013-2018 the Alfalfa authors
                       and the Massachusetts Institute of Technology

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

      1. Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

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
       << " [-d, --device CAMERA] [-p, --pixfmt PIXEL_FORMAT] [-f, --fullscreen]"
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
  bool fullscreen = false;

  const option command_line_options[] = {
    { "device",     required_argument, nullptr, 'd' },
    { "pixfmt",     required_argument, nullptr, 'p' },
    { "fullscreen", no_argument,       nullptr, 'f' },
    { 0, 0, 0, 0 }
  };

  while ( true ) {
    const int opt = getopt_long( argc, argv, "d:p:f", command_line_options, nullptr );

    if ( opt == -1 ) {
      break;
    }

    switch ( opt ) {
    case 'd': camera_device = optarg; break;
    case 'p': pixel_format = optarg; break;
    case 'f': fullscreen = true; break;

    default: usage( argv[ 0 ] ); return EXIT_FAILURE;
    }
  }

  Camera camera { 1280, 720, PIXEL_FORMAT_STRS.at( pixel_format ), camera_device };

  RasterHandle r { MutableRasterHandle{ 1280, 720 } };
  VideoDisplay display { r, fullscreen };

  while ( true ) {
    auto raster = camera.get_next_frame();

    if ( raster.initialized() ) {
      display.draw( raster.get() );
    }
  }

  return EXIT_SUCCESS;
}
