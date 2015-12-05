#include <string>
#include <sysexits.h>

#include "alfalfa_video.hh"
#include "filesystem.hh"
#include "player.hh"
#include <functional>

using namespace std;

/* Takes an Alfalfa video and an encoded version of it, checks that all of the
   frames are in sync, quality db entries are correct, and the FrameDB is
   intact. Also checks that they are combinable (same raster lists). */
void alfalfa_encode_test( string original_video_path, string encoded_video_path ) {
  size_t track_id = 0;
  size_t raster_index = 0;

  PlayableAlfalfaVideo original( original_video_path );
  PlayableAlfalfaVideo encoded( encoded_video_path );

  if ( not original.can_combine( encoded ) ) {
    throw invalid_argument( "uncombinable videos." );
  }

  FramePlayer original_player( original.get_info().width, original.get_info().height );
  FramePlayer encoded_player( encoded.get_info().width, encoded.get_info().height );

  auto original_frames = original.get_frames( track_id );
  auto encoded_frames = encoded.get_frames( track_id );

  while ( encoded_frames.first != encoded_frames.second ) {
    auto encoded_raster = encoded_player.decode( encoded.get_chunk( *encoded_frames.first ) );

    if ( encoded_frames.first->target_hash().shown ) {
      // original_player should go to the next shown frame
      while ( original_frames.first != original_frames.second and
             not original_frames.first->target_hash().shown ) {
        original_player.decode( original.get_chunk( *original_frames.first ) );
        original_frames.first++;
      }

      if ( original_frames.first == original_frames.second ) {
        throw invalid_argument( "encoded video is inconsistent with the original." );
      }

      if ( not original.equal_raster_lists( encoded ) ) {
        throw logic_error( "original rasters don't line up." );
      }

      auto original_raster = original_player.decode( original.get_chunk( *original_frames.first ) );

      double quality = original_raster.get().get().quality( encoded_raster.get().get() );
      double expected_quality = encoded.get_quality( raster_index++, *encoded_frames.first );

      if ( abs( quality - expected_quality ) > 1e-2 ) {
        throw logic_error( "invalid quality data." );
      }

      original_frames.first++;
    }

    if ( not encoded.has_frame_name( encoded_frames.first->name() ) ) {
      throw logic_error( "invalid framedb." );
    }

    encoded_frames.first++;
  }
}

int main(int argc, char const *argv[]) {
  try {
    if ( argc != 3 ) {
      cerr << "Usage: " << argv[ 0 ] << " <original-video> <encoded-video>" << endl;
      return EX_USAGE;
    }

    string original_video_path( argv[ 1 ] );
    string encoded_video_path( argv[ 2 ] );

    alfalfa_encode_test( original_video_path, encoded_video_path );
  } catch (const exception &e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
