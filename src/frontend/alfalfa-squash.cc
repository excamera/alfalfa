#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>

#include <iostream>
#include <fstream>
#include <string>

#include "exception.hh"
#include "alfalfa_video.hh"
#include "manifests.hh"
#include "frame_db.hh"
#include "tracking_player.hh"
#include "ivf_writer.hh"

using namespace std;

int main( int argc, char const *argv[] )
{
  try
  {
    if ( argc <= 4 ) {
      cerr << "Usage: " << argv[ 0 ] << " <alf-1> <alf-2> <destination-dir> <track-id-mappings>..." << endl;
      return EX_USAGE;
    }

    string alf1_path( argv[ 1 ] );
    string alf2_path( argv[ 2 ] );
    string destination_dir( argv[ 3 ] );

    map<size_t, size_t> track_id_mapping;
    for ( int i = 4; i < argc; i++ ) {
      string mapping( argv[ i ] );
      size_t delim_pos = mapping.find( ':' );
      int track_id1 = stoul( mapping.substr( 0, delim_pos ) );
      int track_id2 = stoul( mapping.substr( delim_pos+1 ) );

      track_id_mapping[ track_id2 ] = track_id1;
    }

    PlayableAlfalfaVideo alf1( alf1_path );
    PlayableAlfalfaVideo alf2( alf2_path );
    WritableAlfalfaVideo res_video( destination_dir, alf1.get_info().width, alf1.get_info().height );

    squash ( res_video, alf1 );
    squash ( res_video, alf2, track_id_mapping );

    res_video.save();
  } catch (const exception &e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
