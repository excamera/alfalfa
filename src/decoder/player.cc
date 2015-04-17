#include "player.hh"
#include "uncompressed_chunk.hh"
#include "decoder_state.hh"
#include <cstdio>

using namespace std;

Player::Player( const string & file_name )
  : file_( file_name ),
    decoder_( file_.width(), file_.height() )
{
  if ( file_.fourcc() != "VP80" ) {
    throw Unsupported( "not a VP8 file" );
  }

  // Start at first KeyFrame
  while ( frame_no_ < file_.frame_count() ) {
    UncompressedChunk uncompressed_chunk( file_.frame( frame_no_ ), 
					  file_.width(), file_.height() );
    if ( uncompressed_chunk.key_frame() ) {
      break;
    }
    frame_no_++;
  }
}

RasterHandle Player::advance( const bool before_loop_filter )
{
  while ( not eof() ) {
    RasterHandle raster( file_.width(), file_.height() );
    if ( decoder_.decode_frame( file_.frame( frame_no_++ ), raster, 
				before_loop_filter ) ) {
      return raster;
    }
  }

  throw Unsupported( "hidden frames at end of file" );
}

bool Player::eof( void ) const
{
  return frame_no_ == file_.frame_count();
}

vector< uint8_t > Player::operator-( Player & source_player )
{
  if ( file_.width() != source_player.file_.width() or
       file_.height() != source_player.file_.height() ) {
    throw Unsupported( "stream size mismatch" );
  }

  unsigned int target_frame_no = frame_no_ - 1;
  unsigned int source_frame_no = source_player.frame_no_ - 1;

  printf("Diffing target %u and source %u\n", target_frame_no, source_frame_no);

  Chunk compressed_target = file_.frame( target_frame_no );
  UncompressedChunk whole_target( compressed_target, file_.width(),
				  file_.height() );

  RasterHandle target_raster( file_.width(), file_.height() );

  vector< uint8_t > serialized_frame;

  DecoderState & target_state = decoder_.state_;

  if ( whole_target.key_frame() ) {
    const KeyFrame parsed_frame = target_state.parse_and_apply<KeyFrame>( whole_target );
    parsed_frame.decode( target_state.segmentation, target_raster );

    serialized_frame = parsed_frame.serialize( target_state.probability_tables );
  }
  else {
    DecoderState & source_state = source_player.decoder_.state_;

    UncompressedChunk whole_source( source_player.file_.frame( source_frame_no ),
				    source_player.file_.width(),
				    source_player.file_.height() );
    RasterHandle source_raster( source_player.file_.width(), source_player.file_.height() );

    const InterFrame source_frame = source_state.parse_and_apply<InterFrame>( whole_source );
    source_frame.decode( source_state.segmentation, source_player.decoder_.references_, source_raster );

    InterFrame target_frame = target_state.parse_and_apply<InterFrame>( whole_target );

    ProbabilityTables target_probability_tables = target_state.probability_tables;
    if ( target_frame.header().refresh_entropy_probs ) {
      target_probability_tables.update( target_frame.header() );
    }

    target_frame.rewrite_as_diff( source_state, target_state, decoder_.references_,
				  source_raster, target_raster );

    target_frame.optimize_continuation_coefficients();

    serialized_frame = target_frame.serialize( target_state.probability_tables );

  }

  fprintf( stderr, "Frame %u, original length: %lu bytes. Diff length: %lu bytes.\n",
	   target_frame_no, compressed_target.size(), serialized_frame.size() );

  return serialized_frame;
}

RasterHandle Player::reconstruct_diff( const std::vector< uint8_t > & diff )
{
  RasterHandle raster( file_.width(), file_.height() );

  // Make a copy, otherwise decode_frame would mess with the internal state of the player
  // and we couldn't loop through diffs
  Decoder test_decoder = decoder_; 

  test_decoder.decode_frame( Chunk( &diff.at( 0 ), diff.size() ), raster );

  return raster;
}
