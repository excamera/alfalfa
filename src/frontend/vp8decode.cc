/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

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
