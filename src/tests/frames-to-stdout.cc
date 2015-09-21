#include "player.hh"

#include <fstream>

using namespace std;

int main( int argc, char * argv[] )
{
  if ( argc < 2 ) {
    cerr << "Usage: MANIFEST" << endl;
    return EXIT_FAILURE;
  }

  ifstream manifest( argv[ 1 ] );

  uint16_t width, height;
  manifest >> width >> height;

  FramePlayer player( width, height );
  Optional<FramePlayer> source_copy;

  while ( not manifest.eof() ) {
    string frame_name;
    manifest >> frame_name;

    if ( frame_name == "" ) {
      // Silly last line edge case
      break;
    }
    Optional<RasterHandle> raster;
    SerializedFrame frame( frame_name );

    raster = player.decode( frame );
    if ( raster.initialized() ) {
      raster.get().get().dump( stdout );
    }
    
  }
}
