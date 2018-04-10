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

#include <cstdio>
#include <cstring>
#include <getopt.h>
#include <iostream>

#include "file_descriptor.hh"
#include "optional.hh"
#include "player.hh"
#include "yuv4mpeg.hh"

using namespace std;

int usage(char *argv0);

int main( int argc, char *argv[] )
{
  try {
    if ( argc < 2 ) {
      return usage(argv[0]);
    }

    Optional<FileDescriptor> y4m_fd;
    char *decoder_state = NULL;

    while (true) {
      const int opt = getopt(argc, argv, "s:o:");

      if (opt == -1) {
        break;
      }

      switch ((char) opt) {
        case 's':
          decoder_state = optarg;
          break;

        case 'o':
          y4m_fd.initialize(fopen(optarg, "wb"));
          break;

        default:
          return usage(argv[0]);
      }
    }

    if (optind >= argc) {
      return usage(argv[0]);
    }

    Player player = decoder_state == NULL
      ? Player( argv[optind] )
      : EncoderStateDeserializer::build<Player>(decoder_state, argv[optind]);

    while ( not player.eof() ) {
      RasterHandle raster = player.advance();

      if (y4m_fd.initialized()) {
        if (lseek(y4m_fd.get().fd_num(), 0, SEEK_CUR) == 0) {
          // position 0: we haven't written a header yet
          y4m_fd.get().write(YUV4MPEGHeader(raster).to_string());
        }

        YUV4MPEGFrameWriter::write(raster, y4m_fd.get());
      }
    }

  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int usage(char *argv0) {
  cerr << "Usage: " << argv0 << " [-s decoder_state] [-o y4m_output] input_file" << endl;
  return EXIT_FAILURE;
}
