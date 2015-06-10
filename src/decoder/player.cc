#include "player.hh"
#include "uncompressed_chunk.hh"
#include "decoder_state.hh"
#include "diff_generator.hh"
#include "serialized_frame.hh"

#include <fstream>

using namespace std;

template<class DecoderType>
FramePlayer<DecoderType>::FramePlayer( const uint16_t width, const uint16_t height )
  : width_( width ),
    height_( height )
{}

template<class DecoderType>
Optional<RasterHandle> FramePlayer<DecoderType>::decode( const SerializedFrame & frame )
{
  if ( not frame.validate_source( decoder_ ) ) {
    throw Invalid( "Decoded frame from incorrect state" );
  }

  Optional<RasterHandle> raster = decoder_.decode_frame( frame.chunk() );

  if ( not frame.validate_target( decoder_ ) ) {
    throw Invalid( "Target doesn't match after decode: " + decoder_.hash_str() );
  }

  return raster;
}
  
template<class DecoderType>
bool FramePlayer<DecoderType>::can_decode( const SerializedFrame & frame ) const
{
  return frame.validate_source( decoder_ );
}

template<class DecoderType>
const Raster & FramePlayer<DecoderType>::example_raster( void ) const
{
  return decoder_.example_raster();
}

template<class DecoderType>
bool FramePlayer<DecoderType>::equal_references( const FramePlayer & other ) const
{
  return decoder_.equal_references( other.decoder_ );
}

template<class DecoderType>
void FramePlayer<DecoderType>::update_continuation( const FramePlayer & other )
{
  return decoder_.update_continuation( other.decoder_ );
}

template<>
void FramePlayer<DiffGenerator>::set_references( bool set_last, bool set_golden, bool set_alt,
                                                 const FramePlayer & target_player )
{
  decoder_.set_references( set_last, set_golden, set_alt, target_player.decoder_ );
}

template<>
ReferenceTracker FramePlayer<DiffGenerator>::updated_references( void ) const
{
  return decoder_.updated_references();
}

template<>
ReferenceTracker FramePlayer<DiffGenerator>::missing_references( const FramePlayer & other ) const
{
  return decoder_.missing_references( other.decoder_ );
}

template<>
string FramePlayer<DiffGenerator>::cur_frame_stats( void ) const
{
  return decoder_.cur_frame_stats();
}

template<>
SerializedFrame FramePlayer<DiffGenerator>::operator-( const FramePlayer & source_player ) const
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
    Optional<RasterHandle> raster = this->decoder_.decode_frame( file_.frame( frame_no_++ ) );
    if ( raster.initialized() ) {
      displayed_frame_no_++;

      return raster.get();
    }
  }

  throw Unsupported( "hidden frames at end of file" );
}

template<>
SerializedFrame FilePlayer<DiffGenerator>::serialize_next( void )
{
  Decoder source = this->decoder_;
  Chunk frame = file_.frame( frame_no_++ );

  Optional<RasterHandle> raster = this->decoder_.decode_frame( frame );

  if ( raster.initialized() ) {
    displayed_frame_no_++;
  }

  SerializedFrame s_frame( frame, raster.initialized(), this->decoder_.source_hash_str( source ), this->decoder_.target_hash_str() );

  if ( raster.initialized() ) {
    s_frame.set_output( raster.get() );
  } /* XXX need to account for unset output in SerializedFrame */

  return s_frame;
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
