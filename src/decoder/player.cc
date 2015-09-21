#include "player.hh"
#include "uncompressed_chunk.hh"
#include "decoder_state.hh"
#include "serialized_frame.hh"

#include <fstream>

using namespace std;

FramePlayer::FramePlayer( const uint16_t width, const uint16_t height )
  : width_( width ),
    height_( height ),
    decoder_( width, height )
{}

Optional<RasterHandle> FramePlayer::decode( const Chunk & chunk )
{
  return decoder_.decode_frame( chunk );
}

Optional<RasterHandle> FramePlayer::decode( const SerializedFrame & frame )
{
  assert( frame.validate_source( decoder_.get_hash() ) );

  Optional<RasterHandle> raster = decode( frame.chunk() );

  assert( frame.validate_target( decoder_.get_hash() ) );

  return raster;
}
  
bool FramePlayer::can_decode( const SerializedFrame & frame ) const
{
  // FIXME shouldn't have to fully hash this?
  return frame.validate_source( decoder_.get_hash() );
}

const Raster & FramePlayer::example_raster( void ) const
{
  return decoder_.example_raster();
}

bool FramePlayer::operator==( const FramePlayer & other ) const
{
  return decoder_ == other.decoder_;
}

bool FramePlayer::operator!=( const FramePlayer & other ) const
{
  return not operator==( other );
}

ostream& operator<<( ostream & out, const FramePlayer & player)
{
  return out << player.decoder_.get_hash().str();
}

FilePlayer::FilePlayer( const string & file_name )
  : FilePlayer( IVF( file_name ) )
{}

FilePlayer::FilePlayer( IVF && file )
  : FramePlayer( file.width(), file.height() ),
    file_ ( move( file ) )
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

Chunk FilePlayer::get_next_frame( void )
{
  return file_.frame( frame_no_++ );
}

RasterHandle FilePlayer::advance( void )
{
  while ( not eof() ) {
    Optional<RasterHandle> raster = decode( get_next_frame() );
    if ( raster.initialized() ) {
      return raster.get();
    }
  }

  throw Unsupported( "hidden frames at end of file" );
}

bool FilePlayer::eof( void ) const
{
  return frame_no_ == file_.frame_count();
}

long unsigned int FilePlayer::original_size( void ) const
{
  return file_.frame( cur_frame_no() ).size();
}
