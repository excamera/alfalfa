#include <iostream>

#include "player.hh"

int main( int argc, char * argv[] )
{
  Player source_player( argv[ 1 ] );
  Player target_player( argv[ 2 ] );

  if ( source_player.width() != target_player.width()
       or source_player.height() != target_player.height() ) {
    throw Unsupported( "stream size mismatch" );
  }

  while ( true ) {
    RasterHandle source_raster = source_player.new_raster();
    RasterHandle target_raster = target_raster.new_raster();

    if ( !source_player.next_shown_frame( source_raster, true ) ) {
      // EOF
      break;
    }

    if ( !target_player.next_shown_frame( target_raster, true ) ) {
    }

  }

  return 0;
}
