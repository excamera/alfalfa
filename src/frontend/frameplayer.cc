#include "player.hh"
#include "display.hh"

using namespace std;

int main( int argc, char * argv[] )
{
  if ( argc < 4 ) {
    cerr << "Usage: WIDTH HEIGHT START_FRAME [ OTHER_FRAMES ... ]";
    return EXIT_FAILURE;
  }

  FramePlayer<Decoder> player( stoi( argv[ 0 ] ), stoi( argv[ 1 ] ) );

  for ( int arg_idx = 3; arg_idx < argc; arg_idx++ ) {
    SerializedFrame frame( argv[ arg_idx ] );

    RasterHandle raster = player.decode( frame );
  }
}
