#include "serialized_frame.hh"

#include <fstream>
#include <sstream>

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

SerializedFrame::SerializedFrame( const string & path )
  : frame_(),
    source_hash_(),
    target_hash_(),
    shown_()
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

  size_t output_pos = path.find( "#", separator_pos + 1 );

  source_hash_ = string( path, name_position, separator_pos );
  target_hash_ = string( path, separator_pos + 1, output_pos - separator_pos - 1 );

  ifstream file( path, ifstream::binary );
  file.seekg( 0, file.end );
  int size = file.tellg();
  file.seekg( 0, file.beg );

  frame_.resize( size );
  file.read( reinterpret_cast<char *>(frame_.data()), size );
}

SerializedFrame::SerializedFrame( const vector<uint8_t> & frame,
                                  bool shown,
				  const string & source_hash,
				  const string & target_hash )
  : frame_( frame ),
    source_hash_( source_hash ),
    target_hash_( target_hash ),
    shown_( shown )
{}

SerializedFrame::SerializedFrame( const Chunk & frame,
                                  bool shown,
				  const string & source_hash,
				  const string & target_hash )
  : SerializedFrame( vector<uint8_t>( frame.buffer(), frame.buffer() + frame.size() ),
		     shown, source_hash, target_hash )
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
  stringstream name_stream;
  name_stream << hex << uppercase << source_hash_ << "#" << target_hash_ << 
              "#";
  if ( output_raster_.initialized() ) {
    name_stream << get_output().hash();
  }
  else {
    name_stream << "x";
  }

  return name_stream.str();
}

double SerializedFrame::psnr( const RasterHandle & original ) const
{
  assert( output_raster_.initialized() );
  return original.get().psnr( output_raster_.get() );
}

RasterHandle SerializedFrame::get_output( void ) const
{
  return output_raster_.get();
}

void SerializedFrame::set_output( const RasterHandle & raster )
{
  output_raster_.initialize( raster );
}

unsigned SerializedFrame::size( void ) const
{
  return frame_.size();
}

void SerializedFrame::write( const string & path ) const
{
  ofstream file( path + name(), ofstream::binary );

  file.write( reinterpret_cast<const char *>(frame_.data()), frame_.size() );
}
