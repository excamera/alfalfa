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

    AlfalfaVideo source_video( source_path, OpenMode::READ );

    FileSystem::create_directory( destination_dir );
    AlfalfaVideo alfalfa_video( destination_dir, OpenMode::TRUNCATE );

    vector<string> vpxenc_args;
    vpxenc_args.push_back( "vpxenc" );

    std::for_each( argv + 4, argv + argc, [ & vpxenc_args ]( const char * arg ) {
        vpxenc_args.push_back( arg );
      }
    );

    string encoded_file = source_video.encode( track_id, vpxenc_args );
    string new_ivf_filename = alfalfa_video.get_ivf_file_name();
    {
      IVF ivf_file( encoded_file );

      IVFWriter imported_ivf_file( FileSystem::append( destination_dir, new_ivf_filename ),
        ivf_file.fourcc(),
        ivf_file.width(),
        ivf_file.height(),
        ivf_file.frame_rate(),
        ivf_file.time_scale() );

      /* copy each frame */
      for ( unsigned int i = 0; i < ivf_file.frame_count(); i++ ) {
        imported_ivf_file.append_frame( ivf_file.frame( i ) );
      }
    }

    alfalfa_video.import( new_ivf_filename,
      make_optional( true, source_video ), track_id );
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
