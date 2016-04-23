#include <fstream>
#include <algorithm>
#include <iostream>
#include <string>

#include "dct.hh"
#include "frame.hh"
#include "player.hh"
#include "vp8_raster.hh"
#include "decoder.hh"
#include "display.hh"

using namespace std;

KeyFrame make_empty_frame( unsigned int width, unsigned int height ) {
  BoolDecoder data { { nullptr, 0 } };
  KeyFrame frame { true, width, height, data };
  frame.parse_macroblock_headers( data, ProbabilityTables {} );
  return frame;
}

void copy_frames( KeyFrame & target __attribute((unused)), const KeyFrame & source __attribute((unused)) )
{
  /* do nothing */
}

int main( int argc, char *argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc != 3 ) {
      cerr << "Usage: " << argv[ 0 ] << " <video> <frame number>" << endl;
      return EXIT_FAILURE;
    }

    IVF ivf( argv[ 1 ] );
    unsigned int frame_number = atoi( argv[ 2 ] );

    UncompressedChunk uncompressed_chunk { ivf.frame( frame_number ), ivf.width(), ivf.height() };
    if ( not uncompressed_chunk.key_frame() ) {
      throw runtime_error( "not a keyframe" );
    }

    DecoderState state { ivf.width(), ivf.height() };
    References references { ivf.width(), ivf.height() };
    MutableRasterHandle raster { ivf.width(), ivf.height() };

    KeyFrame frame = state.parse_and_apply<KeyFrame>( uncompressed_chunk );

    KeyFrame newframe = make_empty_frame( ivf.width(), ivf.height() );

    copy_frames( newframe, frame );

    newframe.decode( state.segmentation, references, raster );
    newframe.loopfilter( state.segmentation, state.filter_adjustments, raster );

    VideoDisplay display { raster };
    display.draw( raster );

    pause();
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
