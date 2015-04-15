#include "player.hh"
#include "uncompressed_chunk.hh"

Player::Player( const std::string & file_name )
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
