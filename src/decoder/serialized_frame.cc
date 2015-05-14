#include "serialized_frame.hh"

#include <fstream>

using namespace std;

static bool hash_equal( const string & hash_str, const string & other_hash_str )
{
  size_t underscore_pos = -1, other_underscore_pos = -1;

  for ( int i = 0; i < 5; i++ ) {
    size_t old_pos = underscore_pos + 1;
    size_t old_other_pos = other_underscore_pos + 1;

    underscore_pos = hash_str.find( '_', old_pos );
    other_underscore_pos = other_hash_str.find( '_', old_other_pos );

    string sub_str( hash_str, old_pos, underscore_pos - old_pos );
    string other_sub_str( other_hash_str, old_other_pos, other_underscore_pos - old_other_pos );

    if ( sub_str != "x" and other_sub_str != "x" and sub_str != other_sub_str ) {
      return false;
    }
  }

  return true;
}

SerializedFrame::SerializedFrame( const std::string & path )
  : frame_ {},
    source_hash_ {},
    target_hash_ {}
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

  source_hash_ = string( path, name_position, separator_pos );
  target_hash_ = string( path, separator_pos + 1 );

  ifstream file( path, ifstream::binary );
  file.seekg( 0, file.end );
  int size = file.tellg();
  file.seekg( 0, file.beg );

  frame_.resize( size );
  file.read( reinterpret_cast<char *>(frame_.data()), size );
}

SerializedFrame::SerializedFrame( const vector<uint8_t> & frame,
				  const string & source_hash,
				  const string & target_hash )
  : frame_( frame ),
    source_hash_( source_hash ),
    target_hash_( target_hash )
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
  return hash_equal( source_hash_, decoder.hash_str() );
}

bool SerializedFrame::validate_target( const Decoder & decoder ) const
{
  return hash_equal( target_hash_, decoder.hash_str() );
}

string SerializedFrame::name( void ) const
{
  return source_hash_ + "#" + target_hash_;
}

void SerializedFrame::write( const string & path ) const
{
  ofstream file( path + name(), ofstream::binary );

  file.write( reinterpret_cast<const char *>(frame_.data()), frame_.size() );
}
