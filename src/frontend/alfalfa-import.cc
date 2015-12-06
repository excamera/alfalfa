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

void import( WritableAlfalfaVideo & alfalfa_video, const string & filename )
{
  IVF ivf_file( filename );
  TrackingPlayer player( filename );

  unsigned int i = 0;
  size_t track_id = 0;

  while ( not player.eof() ) {
    auto next_frame_data = player.serialize_next();
    FrameInfo next_frame( next_frame_data.first );

    if ( not alfalfa_video.has_frame_name( next_frame.name() ) ) {
      size_t offset = alfalfa_video.append_frame_to_ivf( ivf_file.frame( i ) );
      next_frame.set_offset( offset );
    }

    size_t original_raster = next_frame.target_hash().output_hash;
    double quality = 1.0;

    alfalfa_video.insert_frame( next_frame, original_raster, quality, track_id );
    i++;
  }
}

int main( int argc, char const *argv[] )
{
  try {
    if ( argc != 3 ) {
      cerr << "Usage: " << argv[ 0 ] << " <ivf-file> <destination-dir>" << endl;
      return EX_USAGE;
    }

    const string ivf_file( argv[ 1 ] );
    const IVF ivf( ivf_file );
    const string destination_dir( argv[ 2 ] );

    FileSystem::create_directory( destination_dir );
    WritableAlfalfaVideo alfalfa_video( destination_dir, ivf.fourcc(), ivf.width(), ivf.height() );

    import( alfalfa_video, ivf_file );

    alfalfa_video.save();
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
