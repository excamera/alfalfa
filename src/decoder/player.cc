#include "player.hh"
#include "uncompressed_chunk.hh"
#include "decoder_state.hh"

#include <fstream>

using namespace std;

FramePlayer::FramePlayer( const uint16_t width, const uint16_t height )
  : width_( width ),
    height_( height ),
    decoder_( width, height )
{}

Optional<RasterHandle> FramePlayer::decode( const Chunk & chunk )
{
  return decoder_.parse_and_decode_frame( chunk );
}

Optional<RasterHandle> FramePlayer::safe_decode( const FrameInfo & info, const Chunk & chunk )
{
#ifndef NDEBUG
  DecoderHash correct_hash = decoder_.get_hash();
  if ( not correct_hash.can_decode( info.source_hash() ) ) {
    throw Invalid( "Player cannot decode frame" );
  }

  Optional<RasterHandle> output = decode( chunk );

  DecoderHash real_hash = decoder_.get_hash();
  correct_hash.update( info.target_hash() );

  if ( real_hash != correct_hash ) {
    cerr << real_hash.str() << " " << correct_hash.str() << " from " << info.frame_name() << "\n";
    throw Invalid( "Player incorrectly decoded frame" );
  }

  return output;
#else
  (void)info; // suppress unused warning
  return decode( chunk );
#endif
}

bool FramePlayer::can_decode( const FrameInfo & frame ) const
{
  // FIXME shouldn't have to fully hash this?
  return frame.validate_source( decoder_.get_hash() );
}

const VP8Raster & FramePlayer::example_raster( void ) const
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

FilePlayer::FilePlayer( const string & filename )
  : FilePlayer( filename, IVF( filename ) )
{}

FilePlayer::FilePlayer( const string & filename, IVF && file )
  : FramePlayer( file.width(), file.height() ),
    file_ ( move( file ) ),
    filename_( filename )
{
  if ( file_.fourcc() != "VP80" ) {
    throw Unsupported( "not a VP8 file" );
  }

  // Start at first KeyFrame
  while ( frame_no_ < file_.frame_count() ) {
    UncompressedChunk uncompressed_chunk = decoder_.decompress_frame( file_.frame( frame_no_ ) );
    if ( uncompressed_chunk.key_frame() ) {
      break;
    }
    frame_no_++;
  }
}

FrameRawData FilePlayer::get_next_frame( void )
{
  pair<uint64_t, uint32_t> frame_location = file_.frame_location( frame_no_ );
  return { file_.frame( frame_no_++ ), frame_location.first, frame_location.second };
}

RasterHandle FilePlayer::advance( void )
{
  while ( not eof() ) {
    Optional<RasterHandle> raster = decode( get_next_frame().chunk );
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
