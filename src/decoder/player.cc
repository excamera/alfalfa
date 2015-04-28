#include "player.hh"
#include "uncompressed_chunk.hh"
#include "decoder_state.hh"
#include "diff_generator.hh"

using namespace std;

template< class DecoderType >
GenericPlayer<DecoderType>::GenericPlayer( const string & file_name )
  : file_( make_shared<IVF>( file_name ) ),
    decoder_( file_->width(), file_->height() )
{
  if ( file_->fourcc() != "VP80" ) {
    throw Unsupported( "not a VP8 file" );
  }

  // Start at first KeyFrame
  while ( frame_no_ < file_->frame_count() ) {
    UncompressedChunk uncompressed_chunk( file_->frame( frame_no_ ), 
					  file_->width(), file_->height() );
    if ( uncompressed_chunk.key_frame() ) {
      break;
    }
    frame_no_++;
  }
}

template< class DecoderType >
RasterHandle GenericPlayer<DecoderType>::advance( void )
{
  while ( not eof() ) {
    RasterHandle raster( file_->width(), file_->height() );
    if ( decoder_.decode_frame( file_->frame( frame_no_++ ), raster ) ) {
      return raster;
    }
  }

  throw Unsupported( "hidden frames at end of file" );
}

template< class DecoderType >
bool GenericPlayer<DecoderType>::eof( void ) const
{
  return frame_no_ == file_->frame_count();
}

template<>
long unsigned int GenericPlayer<DiffGenerator>::original_size( void ) const
{
  return file_->frame( frame_no() ).size();
}

template<>
bool GenericPlayer<DiffGenerator>::operator==( const GenericPlayer & other ) const
{
  return decoder_ == other.decoder_;
}

template<>
bool GenericPlayer<DiffGenerator>::operator!=( const GenericPlayer & other ) const
{
  return not operator==( other );
}

template<>
vector< uint8_t > GenericPlayer<DiffGenerator>::operator-( const GenericPlayer & source_player ) const
{
  if ( file_->width() != source_player.file_->width() or
       file_->height() != source_player.file_->height() ) {
    throw Unsupported( "stream size mismatch" );
  }

  return decoder_ - source_player.decoder_;
}

template<>
RasterHandle GenericPlayer<DiffGenerator>::reconstruct_diff( const std::vector< uint8_t > & diff )
{
  RasterHandle raster( file_->width(), file_->height() );

  decoder_.reset_references();

  decoder_.decode_frame( Chunk( &diff.at( 0 ), diff.size() ), raster );

  return raster;
}

template class GenericPlayer< Decoder >;
template class GenericPlayer< DiffGenerator >;
