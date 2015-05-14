#include "player.hh"
#include "display.hh"

#include <fstream>

using namespace std;

int main( int argc, char * argv[] )
{
  if ( argc < 2 ) {
    cerr << "Usage: MANIFEST";
    return EXIT_FAILURE;
  }

  ifstream manifest( argv[ 1 ] );

  uint16_t width, height;
  manifest >> width >> height;

  FramePlayer<Decoder> player( width, height );
  Optional<FramePlayer<Decoder>> source_copy;

  VideoDisplay display { player.example_raster() };

  while ( not manifest.eof() ) {
    char frame_type;
    string frame_name;
    manifest >> frame_type >> frame_name;

    Optional<RasterHandle> raster;
    SerializedFrame frame( frame_name );

    if ( frame_type == 'C' ) {
      /* Copy the player to deal with incomplete diffs */
      if ( not source_copy.initialized() ) {
	source_copy.initialize( player );
      }
      raster = player.decode( frame );
    }
    else if ( frame_type == 'S' ) {
      assert( source_copy.initialized() );

      source_copy.get().decode( frame );

      player.update_continuation( source_copy.get() );
    }
    else {
      raster = player.decode( frame );
    }

    if ( raster.initialized() ) {
      display.draw( raster.get() );
    }
  }
}
