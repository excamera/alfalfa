/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <cstdio>
#include <cstring>
#include <iostream>

#include "file_descriptor.hh"
#include "optional.hh"
#include "player.hh"
#include "yuv4mpeg.hh"

using namespace std;

int main( int argc, char *argv[] )
{
  try {
    if ( argc < 2 ) {
      cerr << "Usage: " << argv[ 0 ] << " FILENAME [decoder_state] [output_file]" << endl;
      return EXIT_FAILURE;
    }

    bool to_y4m = false;
    Optional<FileDescriptor> y4m_fd;
    if (argc > 3) {
      to_y4m = true;
      y4m_fd.initialize(fopen(argv[3], "wb"));
    }

    Player player = argc < 3
      ? Player( argv[ 1 ] )
      : EncoderStateDeserializer::build<Player>(argv[2], argv[1]);

    while ( not player.eof() ) {
      RasterHandle raster = player.advance();

      if (to_y4m) {
        if (lseek(y4m_fd.get().num(), 0, SEEK_CUR) == 0) {
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
