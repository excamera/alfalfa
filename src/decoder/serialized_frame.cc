#include "serialized_frame.hh"

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
  cout << frame_.size() << "\n";
  cout << path + source_desc_ + "#" + target_desc_ << "\n";
  ofstream file( path + source_desc_ + "#" + target_desc_, ofstream::binary );

  file.write( reinterpret_cast<const char *>(frame_.data()), frame_.size() );
}
