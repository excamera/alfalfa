/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Copyright 2013-2019 the Alfalfa authors
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

#include "raster_handle.hh"
#include "display.hh"
#include "file.hh"
#include "jpeg.hh"

using namespace std;

void usage( const char *argv0 )
{
  cerr << "Usage: " << argv0
       << " filename"
       << endl;
}

int main( int argc, char *argv[] )
{
  try {
    /* check the command-line arguments */
    if ( argc < 1 ) { /* for sticklers */
      abort();
    }

    if ( argc != 2 ) {
      usage( argv[ 0 ] );
      return EXIT_FAILURE;
    }

    /* open file */
    File file { argv[ 1 ] };
    JPEGDecompresser decompresser;

    decompresser.begin_decoding( file.chunk() );

    MutableRasterHandle r { decompresser.width(), decompresser.height() };

    decompresser.decode( r.get() );

    VideoDisplay display { r };

    display.draw( r );

    pause();

    return EXIT_SUCCESS;
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }
}
