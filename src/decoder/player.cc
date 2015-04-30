#include "player.hh"
#include "uncompressed_chunk.hh"
#include "decoder_state.hh"
#include "diff_generator.hh"

using namespace std;

template<class DecoderType>
FramePlayer<DecoderType>::FramePlayer( const uint16_t width, const uint16_t height )
  : width_( width ),
    height_( height )
{}

template<class DecoderType>
RasterHandle FramePlayer<DecoderType>::decode( const vector<uint8_t> & raw_frame,
					       const string & name )
{
  if ( name == "" ) { // dummy test...
    // throw(InvalidFrame) ??
  }

  RasterHandle raster( width_, height_ );

  decoder_.decode_frame( Chunk( &raw_frame.at( 0 ), raw_frame.size() ), raster );

  return raster;
}

template<class DecoderType>
const Raster & FramePlayer<DecoderType>::example_raster( void ) const
{
  return decoder_.example_raster();
}

template<>
vector< uint8_t > FramePlayer<DiffGenerator>::operator-( const FramePlayer & source_player ) const
{
  if ( width_ != source_player.width_ or
       height_ != source_player.height_ ) {
    throw Unsupported( "stream size mismatch" );
  }

  return decoder_ - source_player.decoder_;
}

template<class DecoderType>
bool FramePlayer<DecoderType>::operator==( const FramePlayer & other ) const
{
  return decoder_ == other.decoder_;
}

template<class DecoderType>
bool FramePlayer<DecoderType>::operator!=( const FramePlayer & other ) const
{
  return not operator==( other );
}

template<class DecoderType>
FilePlayer<DecoderType>::FilePlayer( const string & file_name )
  : FilePlayer( IVF( file_name ) )
{}

template<class DecoderType>
FilePlayer<DecoderType>::FilePlayer( IVF && file )
  : FramePlayer<DecoderType>( file.width(), file.height() ),
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

template<class DecoderType>
RasterHandle FilePlayer<DecoderType>::advance( void )
{
  while ( not eof() ) {
    RasterHandle raster( file_.width(), file_.height() );
    if ( this->decoder_.decode_frame( file_.frame( frame_no_++ ), raster ) ) {
      return raster;
    }
  }

  throw Unsupported( "hidden frames at end of file" );
}

template<class DecoderType>
bool FilePlayer<DecoderType>::eof( void ) const
{
  return frame_no_ == file_.frame_count();
}

template<>
long unsigned int FilePlayer<DiffGenerator>::original_size( void ) const
{
  return file_.frame( cur_frame_no() ).size();
}

template class FramePlayer< Decoder >;
template class FramePlayer< DiffGenerator >;
template class FilePlayer< Decoder >;
template class FilePlayer< DiffGenerator >;
