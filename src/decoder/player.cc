#include "player.hh"
#include "uncompressed_chunk.hh"
#include "decoder_state.hh"
#include "diff_generator.hh"

#include <fstream>

using namespace std;

SerializedFrame::SerializedFrame( const std::string & path )
  : frame_ {},
    source_desc_ {},
    target_desc_ {}
{
  size_t name_position = path.rfind("/");

  if ( name_position == string::npos ) {
    name_position = 0;
  } else {
    name_position++;
  }

  size_t separator_pos = path.find( "#", name_position );

  if ( separator_pos == string::npos ) {
    throw internal_error( "Decoding frame", "invalid filename" );
  }

  source_desc_ = string( path, name_position, separator_pos - 1 );
  target_desc_ = string( path, separator_pos + 1 );

  basic_ifstream<uint8_t> file( path );
  file.seekg( 0, file.end );
  int size = file.tellg();
  file.seekg( 0, file.beg );

  frame_.reserve( size );
  file.read( frame_.data(), size );
}

SerializedFrame::SerializedFrame( const vector<uint8_t> & frame,
				  const string & source_hash,
				  const string & target_hash )
  : frame_( frame ),
    source_desc_( source_hash ),
    target_desc_( target_hash )
{}

SerializedFrame::SerializedFrame( const Chunk & frame,
				  const string & source_hash,
				  const string & target_hash )
  : SerializedFrame( vector<uint8_t>( frame.buffer(), frame.buffer() + frame.size() ),
		     source_hash, target_hash )
{}

Chunk SerializedFrame::chunk( void ) const
{
  return Chunk( &frame_.at( 0 ), frame_.size() );
}

bool SerializedFrame::validate_source( const Decoder & decoder ) const
{
  return source_desc_ == decoder.hash_str();
}

bool SerializedFrame::validate_target( const Decoder & decoder ) const
{
  return target_desc_ == decoder.hash_str();
}

void SerializedFrame::write( string path ) const
{
  basic_ofstream<uint8_t> file( path + source_desc_ + "#" + target_desc_ );

  file.write( frame_.data(), frame_.size() );
}

template<class DecoderType>
FramePlayer<DecoderType>::FramePlayer( const uint16_t width, const uint16_t height )
  : width_( width ),
    height_( height )
{}

template<class DecoderType>
RasterHandle FramePlayer<DecoderType>::decode( const SerializedFrame & frame )
{
  if ( not frame.validate_source( decoder_ ) ) {
    throw Unsupported( "Decoded frame from incorrect state" );
  }

  RasterHandle raster( width_, height_ );

  decoder_.decode_frame( frame.chunk(), raster );

  if ( not frame.validate_target( decoder_ ) ) {
    throw Unsupported( "Target doesn't match after decode" );
  }

  return raster;
}

template<class DecoderType>
const Raster & FramePlayer<DecoderType>::example_raster( void ) const
{
  return decoder_.example_raster();
}

template<class DecoderType>
std::string FramePlayer<DecoderType>::hash_str( void ) const
{
  return decoder_.hash_str();
}

template<>
SerializedFrame FramePlayer<DiffGenerator>::operator-( const FramePlayer & source_player ) const
{
  if ( width_ != source_player.width_ or
       height_ != source_player.height_ ) {
    throw Unsupported( "stream size mismatch" );
  }

  vector<uint8_t> diff = decoder_ - source_player.decoder_;

  return SerializedFrame( diff, source_player.hash_str(), hash_str() );
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
    RasterHandle raster( this->width_, this->height_ );
    if ( this->decoder_.decode_frame( file_.frame( frame_no_++ ), raster ) ) {
      return raster;
    }
  }

  throw Unsupported( "hidden frames at end of file" );
}

template<class DecoderType>
SerializedFrame FilePlayer<DecoderType>::serialize_next( void )
{
  string source_desc = this->hash_str();
  Chunk frame = file_.frame( frame_no_++ );

  RasterHandle throwaway_raster( this->width_, this->height_ );

  this->decoder_.decode_frame( frame, throwaway_raster );

  return SerializedFrame( frame, source_desc, this->hash_str() );
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
