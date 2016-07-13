/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <utility>

#include "ivf.hh"
#include "uncompressed_chunk.hh"
#include "decoder_state.hh"
#include "decoder.hh"

using namespace std;

int main( int argc, char * argv[] ) {
  if ( argc != 2 ) {
    cerr << "Usage: " << argv[ 0 ] << " FILENAME" << endl;
    return EXIT_FAILURE;
  }

  IVF file( argv[ 1 ] );

  if ( file.fourcc() != "VP80" ) {
    cerr << argv[ 1 ]  << "is not a VP8 file\n";
    return EXIT_FAILURE;
  }

  uint32_t frame_no = 0;

  // Find key frame
  while ( frame_no < file.frame_count() ) {
    UncompressedChunk uncompressed_chunk( file.frame( frame_no ), file.width(), file.height() );
    if ( uncompressed_chunk.key_frame() ) {
      break;
    }
    frame_no++;
  }

  unordered_map<size_t, vector<DecoderState>> decoder_hashes;
  unordered_set<size_t> potential_state_collisions;

  DecoderState decoder_state( file.width(), file.height() );

  for ( uint32_t i = frame_no; i < file.frame_count(); i++ ) {
    UncompressedChunk frame_chunk( file.frame( i ), file.width(), file.height() );
    if ( frame_chunk.key_frame() ) {
      decoder_state.parse_and_apply<KeyFrame>( frame_chunk );
    } else {
      decoder_state.parse_and_apply<InterFrame>( frame_chunk );
    }

    size_t hash = decoder_state.hash();

    if ( decoder_hashes.count( hash ) ) {
      potential_state_collisions.insert( hash );
    }
    else {
      decoder_hashes.insert( make_pair( hash, vector<DecoderState>() ) );
    }
  }

  DecoderState check_state( file.width(), file.height() );
  for ( uint32_t i = frame_no; i < file.frame_count(); i++ ) {
    UncompressedChunk frame_chunk( file.frame( i ), file.width(), file.height() );
    if ( frame_chunk.key_frame() ) {
      check_state.parse_and_apply<KeyFrame>( frame_chunk );
    } else {
      check_state.parse_and_apply<InterFrame>( frame_chunk );
    }

    size_t hash = check_state.hash();

    if ( potential_state_collisions.count( hash ) ) {
      for ( const DecoderState & collision_state : decoder_hashes[ hash ] ) {
        if ( not ( collision_state == check_state ) ) {
          cerr << "collision on frame number " << i << " hash " << hash << "\n";
          return EXIT_FAILURE;
        }
      }
      decoder_hashes[ hash ].push_back( check_state );
    }
  }
}
