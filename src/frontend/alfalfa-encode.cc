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

void import( WritableAlfalfaVideo & writable,
             const string & filename,
             PlayableAlfalfaVideo & original,
             const size_t ref_track_id )
{
  IVF ivf_file( filename );
  TrackingPlayer player( filename );

  FramePlayer original_player( original.get_info().width, original.get_info().height );
  auto track_frames = original.get_frames( ref_track_id );

  unsigned int i = 0;
  size_t track_id = 0;

  while ( not player.eof() ) {
    auto next_frame_data = player.serialize_next();
    FrameInfo next_frame( next_frame_data.first );

    if ( not writable.has_frame_name( next_frame.name() ) ) {
      size_t offset = writable.append_frame_to_ivf( ivf_file.frame( i ) );
      next_frame.set_offset( offset );
    }

    size_t original_raster = next_frame.target_hash().output_hash;
    double quality = 1.0;

    if ( next_frame.target_hash().shown ) {
      assert(  next_frame.target_hash().shown == next_frame_data.second.initialized() );

      while ( track_frames.first != track_frames.second and not track_frames.first->shown() ) {
        original_player.decode( original.get_chunk( *track_frames.first ) );
        track_frames.first++;
      }

      if ( track_frames.first != track_frames.second ) {
        auto original_uncompressed_frame = original_player.decode(
          original.get_chunk( *track_frames.first ) );

        quality = original_uncompressed_frame.get().get().quality(
          next_frame_data.second.get().get() );

        original_raster = track_frames.first->target_hash().output_hash;

        track_frames.first++;
      }
      else {
        throw logic_error( "bad original video" );
      }
    }

    writable.insert_frame( next_frame, original_raster, quality, track_id );
    i++;
  }
}


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

    IVF encoded = IVF( encoded_file.name() );
    WritableAlfalfaVideo alfalfa_video( destination_dir, encoded.fourcc(), encoded.width(), encoded.height() );
    import( alfalfa_video, encoded_file.name(), source_video, track_id );

    alfalfa_video.save();
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
