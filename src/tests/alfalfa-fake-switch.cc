#include <iostream>

#include "player.hh"
#include "alfalfa_video.hh"

using namespace std;

int main( int argc, char * argv[] )
{
  try {
    if ( argc < 3 ) {
      cerr << "Usage: " << argv[ 0 ] << " input_alf switch_num" << endl;
      return EXIT_FAILURE;
    }

    PlayableAlfalfaVideo alf( argv[ 1 ] );
    unsigned int switch_frame = stoi( argv[ 2 ] );

    FramePlayer source_player( alf.get_info().width, alf.get_info().height );
    FramePlayer target_player( alf.get_info().width, alf.get_info().height );

    auto source_track = alf.get_frames( 0 );
    auto target_track = alf.get_frames( 1 );

    TrackDBIterator source_frame = source_track.first;
    TrackDBIterator target_frame = target_track.first;

    for ( unsigned cur_displayed = 0; cur_displayed <= switch_frame; cur_displayed++ ) {
      Optional<RasterHandle> output;
      while ( not output.initialized() ) {
        output = source_player.decode( alf.get_chunk( *source_frame ) );
        source_frame++;
      }
      output.get().get().dump( stdout );
    }

    /* Catch up the target player to right before the switch */
    for ( unsigned cur_displayed = 0; cur_displayed < switch_frame; cur_displayed++ ) {
      Optional<RasterHandle> output;
      while ( not output.initialized() ) {
        output = target_player.decode( alf.get_chunk( *target_frame ) );
        target_frame++;
      }
    }

    /* Dump all the target rasters after the switch */
    while ( target_frame != target_track.second ) {
      Optional<RasterHandle> output = target_player.decode( alf.get_chunk( *target_frame ) );
      if ( output.initialized() ) {
        output.get().get().dump( stdout );
      }
      target_frame++;
    }
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }
}
