#include "player.hh"
#include "uncompressed_chunk.hh"

Player::Player( const std::string & file_name )
  : file_( file_name ),
    width_( file_.width() ), height_( file_.height() )
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

bool Player::next_shown_frame( RasterHandle & raster, bool preloop )
{
  while ( frame_no_ < file_.frame_count() ) {
    if ( decoder_.decode_frame( file_.frame( frame_no_++ ), raster, preloop ) ) {
      return true;
    }
    // Previous undisplayed frame could be used as a reference,
    // so we can't overwrite it
    raster = new_raster();
  }

  // EOF
  return false;
}

RasterHandle Player::new_raster( void )
{
  return RasterHandle( width_, height_ );
}
