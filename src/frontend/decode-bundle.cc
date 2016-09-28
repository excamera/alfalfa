/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>

#include "file_descriptor.hh"
#include "optional.hh"
#include "player.hh"
#include "yuv4mpeg.hh"

using namespace std;

/*
   xc-decode-bundle: decodes a sequence of IVF files whose
   filenames are given on standard input,
   to a YUV4MPEG video on standard output
*/

int main( int argc, char *argv[] )
{
  try {
    if ( argc > 2 ) {
      cerr << "Usage: " << argv[ 0 ] << " [starting_state]" << endl;
      return EXIT_FAILURE;
    }

    FileDescriptor stdout( STDOUT_FILENO );
    unique_ptr<FramePlayer> player;

    while ( true ) {
      /* read filename */
      string filename;
      getline( cin, filename );
      if ( not cin.good() ) {
        break;
      }

      /* open file */
      cerr << "Opening " << filename << "... ";
      IVF ivf { filename };
      cerr << "done (" << ivf.frame_count() << " frames).\n";

      /* initialize player and output if necessary */
      if ( not player ) {
        cerr << "Initializing with size " << ivf.width() << "x" << ivf.height() << "\n";
        if (argc > 1) {
          player.reset( new FramePlayer { move( EncoderStateDeserializer::build<FramePlayer>(argv[1]) ) } );
          assert(ivf.width() == player->width());
          assert(ivf.height() == player->height());
        } else {
          player.reset( new FramePlayer { ivf.width(), ivf.height() } );
        }

        stdout.write( YUV4MPEGHeader( player->example_raster() ).to_string() );
      }

      if ( not player->current_decoder().minihash_match( ivf.expected_decoder_minihash() ) ) {
        stringstream error;
        error << hex << "Hash mismatch. Expected " << ivf.expected_decoder_minihash()
              << " but decoder is in state " << player->current_decoder().minihash();
        throw Invalid( error.str() );
      }

      /* decode file */
      cerr << filename << " entering state: " << *player << "\n";
      for ( unsigned int frame_no = 0; frame_no < ivf.frame_count(); frame_no++ ) {
        Optional<RasterHandle> raster = player->decode( ivf.frame( frame_no ) );
        if ( raster.initialized() ) {
          YUV4MPEGFrameWriter::write( raster.get(), stdout );
        }
      }
      cerr << filename << " exiting state: " << *player << "\n";
    }
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
