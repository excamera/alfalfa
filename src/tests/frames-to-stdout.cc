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
    char frame_type;
    string frame_name;
    manifest >> frame_type >> frame_name;

    if ( frame_name == "" ) {
      // Silly last line edge case
      break;
    }
    Optional<RasterHandle> raster;
    SerializedFrame frame( frame_name );

    if ( frame_type == 'C' ) {
      /* Copy the player to deal with incomplete diffs */
      if ( not source_copy.initialized() ) {
	source_copy.initialize( player );
      }
      raster = player.decode( frame );
      if ( raster.initialized() ) {
        raster.get().get().dump( stdout );
      }
    }
    else if ( frame_type == 'S' ) {
      assert( source_copy.initialized() );

      source_copy.get().decode( frame );

      player.sync_continuation_raster( source_copy.get() );
    }
    else {
      raster = player.decode( frame );
      if ( raster.initialized() ) {
        raster.get().get().dump( stdout );
      }
    }
  }
}
