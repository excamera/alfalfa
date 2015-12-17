#include <sysexits.h>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <chrono>
#include <tuple>
#include <exception>
#include <boost/variant.hpp>
#include <boost/format.hpp>

#include "alfalfa_player.hh"
#include "player.hh"
#include "display.hh"
#include "2d.hh"

using namespace std;
using namespace std::chrono;

vector<pair<size_t, double> > find_target_frames( AlfalfaVideoProxy & video, size_t raster_index )
{
  size_t target_raster = video.get_raster( raster_index );
  vector<pair<size_t, double> > target_frames;

  for ( auto its = video.get_quality_data_by_original_raster( target_raster );
        its.first != its.second; its.first++ ) {

    auto frames = video.get_frames_by_output_hash( its.first->approximate_raster );

    for( auto itf = frames.first; itf != frames.second; itf++ ) {
      target_frames.push_back( make_pair( itf->frame_id(), its.first->quality ) );
    }
  }

  return target_frames;
}

bool display_frame( VideoDisplay & display,
                    const Optional<RasterHandle> & raster )
{
  if ( raster.initialized() ) {
    display.draw( raster.get() );
    return true;
  }

  return false;
}

int main( int argc, char const *argv[] )
{
  try {
    if ( argc != 2 ) {
      cerr << "Usage: " << argv[ 0 ] << " <alf-video>" << endl;
      return EX_USAGE;
    }

    const string video_dir( argv[ 1 ] );

    AlfalfaVideoProxy video( video_dir );
    AlfalfaPlayer player( video_dir );
    VideoDisplay display( player.example_raster() );

    do {
      cout << "> j <n>\t" << "jump to frame <n>." << endl;

      // Applicable frames to current state
      /*auto search_it = video.get_frames_by_decoder_hash( DecoderHash( 0, 0, 0, 0 ) );
      vector<FrameInfo> applicable_frames( search_it.first, search_it.second );

      if ( applicable_frames.size() > 0 ) {
        cout << "> a\t" << "list of all keyframes ("
             << applicable_frames.size() << ")." << endl;
      }
      else {
        cout << "!! no key frames found." << endl;
      }*/

      cout << "> q\t" << "quit." << endl;

      bool succeeded = false;

      // Showing the menu to the user
      while ( not succeeded ) {
        cout << ">> ";

        string command;
        getline( cin, command );
        istringstream ss( command );

        string arg0;
        ss >> arg0;

        switch ( arg0[ 0 ] ) {
        case 'j': // jump to frame <n>
          {
            if ( command.size() < 2 ) break;

            size_t jump_index;
            ss >> jump_index;

            size_t target_raster = video.get_raster( jump_index );
            auto approximations_search = video.get_quality_data_by_original_raster( target_raster );
            vector<QualityData> approximations( approximations_search.first,
              approximations_search.second );

            size_t index = 0;
            cout << endl;
            for ( auto const & approximation : approximations ) {
              cout << boost::format("%-d) %-X (Q: %-.2f)") % ( index++ ) % approximation.approximate_raster
                      % approximation.quality << endl;
            }

            if ( approximations.size() == 0 ) {
              cout << "!! no frame with this index found." << endl;
              break;
            }

            cout << "[0-" << approximations.size() - 1 << "]? ";
            cin >> index;
            cin.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );

            if ( index >= approximations.size() ) {
              break;
            }

            size_t desired_output = approximations[ index ].approximate_raster;

            RasterHandle raster = player.get_raster( jump_index, desired_output );
            display.draw( raster );

            succeeded = true;
          }

          break;

        case 'n': // next raster in the current track
          break;

        case 'a': // list of all applicable frames
          break;

        case 'q':
          return EXIT_SUCCESS;

        default:
          break;
        }

        if ( not succeeded ) {
          cout << "! invalid command." << endl;
        }
      }

      cout << endl;
    } while ( true );
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
