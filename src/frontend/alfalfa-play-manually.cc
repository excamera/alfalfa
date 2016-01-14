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
      cerr << "Usage: " << argv[ 0 ] << " <server-address>" << endl;
      return EX_USAGE;
    }

    const string server_address( argv[ 1 ] );

    AlfalfaVideoClient video( server_address );
    AlfalfaPlayer player( server_address );
    VideoDisplay display( player.example_raster() );

    do {
      cout << "[ cached items: " << player.cache_size() << " ]" << endl;
      cout << "> j <n>\t" << "jump to frame <n>" << endl;
      cout << "> t <n>\t" << "jump to frame <n> (use a track path)" << endl;
      cout << "> s <n>\t" << "jump to frame <n> (use a switch path)" << endl;
      cout << "> c\t"     << "clear cache" << endl;
      cout << "> q\t" << "quit" << endl;

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
        case 'j':
        case 's':
        case 't':
          {
            if ( command.size() < 2 ) break;

            size_t jump_index;
            ss >> jump_index;

            size_t target_raster = video.get_raster( jump_index );
            vector<QualityData> approximations = video.get_quality_data_by_original_raster( target_raster );

            size_t index = 0;
            cout << endl;
            for ( auto const & approximation : approximations ) {
              cout << boost::format("%-d) %-X (Q: %-.2f)") % ( index++ )
                                                           % approximation.approximate_raster
                                                           % approximation.quality
                   << endl;
            }

            if ( approximations.size() == 0 ) {
              cout << "!! no frame with this index found." << endl;
              break;
            }

            if ( approximations.size() == 1 ) {
              index = 0;
            }
            else {
              cout << "[0-" << approximations.size() - 1 << "]? ";
              cin >> index;
              cin.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );
            }

            if ( index >= approximations.size() ) {
              break;
            }

            size_t desired_output = approximations[ index ].approximate_raster;

            PathType path_type = MINIMUM_PATH;

            if ( arg0[0] == 't' ) path_type = TRACK_PATH;
            else if ( arg0[0] == 's' ) path_type = SWITCH_PATH;

            Optional<RasterHandle> raster = player.get_raster( desired_output, path_type, true );
            display_frame( display, raster );

            succeeded = true;
          }

          break;

        case 'q':
          return EXIT_SUCCESS;

        case 'c':
          player.clear_cache();
          succeeded = true;
          break;

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
