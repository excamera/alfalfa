#include <string>

#include "player.hh"
#include "alfalfa_video.hh"

using namespace std;

int main( int argc, char * argv[] )
{
  try {
    if ( argc < 3 ) {
      cerr << "Usage: " << argv[ 0 ] << " input_alf switch_num\n";
      return EXIT_FAILURE;
    }

    PlayableAlfalfaVideo alf( argv[ 1 ] );

    unsigned switch_num = stoul( argv[ 2 ] );

    FramePlayer player( alf.get_info().width, alf.get_info().height );

    auto track_pair = alf.get_frames( 0 );
    TrackDBIterator cur_frame = track_pair.first;

    unsigned cur_displayed = 0;
    while ( true ) {
      Optional<RasterHandle> raster = player.safe_decode( *cur_frame, alf.get_chunk( *cur_frame ) );
      if ( raster.initialized() ) {
        raster.get().get().dump( stdout );

        if ( cur_displayed == switch_num ) {
          break;
        }

        cur_displayed++;
      }
      cur_frame++;
    }

    /* Maybe?? */
    auto switches = alf.get_frames( cur_frame, 1 );

    for ( SwitchDBIterator switch_iter = switches.first; switch_iter != switches.second; switch_iter++ ) {
      Optional<RasterHandle> raster = player.safe_decode( *switch_iter, alf.get_chunk( *switch_iter ) );

      if ( raster.initialized() ) {
        raster.get().get().dump( stdout );
      }
    }

    TrackDBIterator new_track_start = alf.get_frames( switches.second ).first;

    pair<TrackDBIterator, TrackDBIterator> new_track = alf.get_frames( new_track_start );

    for ( TrackDBIterator frame = new_track.first++; frame != new_track.second; frame++ ) {
      Optional<RasterHandle> raster = player.decode( alf.get_chunk( *frame ) );

      if ( raster.initialized() ) {
        raster.get().get().dump( stdout );
      }
    }
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }
}
