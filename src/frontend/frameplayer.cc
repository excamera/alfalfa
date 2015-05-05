#include "player.hh"
#include "display.hh"

using namespace std;

int main( int argc, char * argv[] )
{
  if ( argc < 2 ) {
    cerr << "Must provide at least one frame to display\n";
    return EXIT_FAILURE;
  }

  FramePlayer<Decoder> player;

  for ( int arg_idx = 1; arg_idx < argc; arg_idx++ ) {
    SerializedFrame frame( argv[ arg_idx ] );

    RasterHandle raster = player.decode( frame );
  }
}
