#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <sysexits.h>

#include "exception.hh"
#include "alfalfa_video.hh"
#include "manifests.hh"
#include "frame_db.hh"
#include "tracking_player.hh"
#include "filesystem.hh"
#include "ivf_writer.hh"
#include "temp_file.hh"

using namespace std;

int main( int argc, char const *argv[] )
{
  try {
    if ( argc < 4 ) {
      cerr << "Usage: " << argv[ 0 ] << " <alf-video> <track-id> <destination-dir> [vpxenc args]" << endl;
      return EX_USAGE;
    }

    const string source_path( argv[ 1 ] );
    const size_t track_id = atoi( argv[ 2 ] );
    const string destination_dir( argv[ 3 ] );

    PlayableAlfalfaVideo source_video( source_path );

    FileSystem::create_directory( destination_dir );

    vector<string> vpxenc_args;
    for_each( argv + 4, argv + argc, [ & vpxenc_args ]( const char * arg ) {
        vpxenc_args.push_back( arg );
      }
    );

    TempFile encoded_file( "encoded" );
    source_video.encode( track_id, vpxenc_args, encoded_file.name() );

    WritableAlfalfaVideo alfalfa_video( destination_dir, IVF( encoded_file.name() ) );
    alfalfa_video.import( encoded_file.name(), source_video, track_id );
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
