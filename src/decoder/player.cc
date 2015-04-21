#include "player.hh"
#include "uncompressed_chunk.hh"
#include "decoder_state.hh"
#include "diff_generator.hh"

using namespace std;

template< class DecoderType >
Player<DecoderType>::Player( const string & file_name )
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

template< class DecoderType >
RasterHandle Player<DecoderType>::advance( void )
{
  while ( not eof() ) {
    RasterHandle raster( file_.width(), file_.height() );
    if ( decoder_.decode_frame( file_.frame( frame_no_++ ), raster ) ) {
      return raster;
    }
  }

  throw Unsupported( "hidden frames at end of file" );
}

template< class DecoderType >
bool Player<DecoderType>::eof( void ) const
{
  return frame_no_ == file_.frame_count();
}

template<>
long unsigned int Player<DiffGenerator>::original_size( void ) const
{
  return file_.frame( frame_no() ).size();
}

template<>
vector< uint8_t > Player<DiffGenerator>::operator-( Player & source_player )
{
  if ( file_.width() != source_player.file_.width() or
       file_.height() != source_player.file_.height() ) {
    throw Unsupported( "stream size mismatch" );
  }

  return decoder_ - source_player.decoder_;
}

template<>
RasterHandle Player<DiffGenerator>::reconstruct_diff( const std::vector< uint8_t > & diff ) const
{
  return decoder_.decode_diff( Chunk( &diff.at( 0 ), diff.size() ) );
}

template class Player< Decoder >;
template class Player< DiffGenerator >;
