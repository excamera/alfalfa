#include <fstream>
#include <algorithm>
#include <iostream>
#include <string>
#include <ivf_writer.cc>

#include "encoder.hh"
#include "dct.hh"
#include "frame.hh"
#include "player.hh"
#include "vp8_raster.hh"
#include "decoder.hh"

using namespace std;

KeyFrame make_empty_frame( unsigned int width, unsigned int height ) {
  BoolDecoder data { { nullptr, 0 } };
  KeyFrame frame { true, width, height, data };
  frame.parse_macroblock_headers( data, ProbabilityTables {} );
  return frame;
}

int main( int argc, char *argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc != 2 ) {
      cerr << "Usage: " << argv[ 0 ] << "[INPUT_VIDEO]" << endl;
      return EXIT_FAILURE;
    }

    IVF ivf( argv[ 1 ] );
    Player player( argv[ 1 ] );
    DecoderState decoder_state( ivf.width(), ivf.height() );

    int frame_number = 3;

    for ( int i = 0; i < frame_number; i++ ) {
      UncompressedChunk whole_frame( ivf.frame( i ), ivf.width(), ivf.height() );
      if ( whole_frame.key_frame() ) {
        decoder_state.parse_and_apply<KeyFrame>( whole_frame );
      }
      else {
        decoder_state.parse_and_apply<InterFrame>( whole_frame );
      }

      player.advance();
    }


    UncompressedChunk whole_frame( ivf.frame( frame_number ), ivf.width(), ivf.height() );

    if ( not whole_frame.key_frame() ) {
      throw domain_error("not a keyframe.");
    }

    KeyFrame frame = decoder_state.parse_and_apply<KeyFrame>( whole_frame );
    RasterHandle raster_handle = player.advance();
    const VP8Raster & raster = raster_handle.get();

    Encoder encoder;
    // ....
    encoder.encode( raster, frame );
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
