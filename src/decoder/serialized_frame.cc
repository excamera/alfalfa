#include "serialized_frame.hh"

#include <fstream>
#include <sstream>

using namespace std;

static string get_name( const string & path )
{
  size_t name_position = path.rfind("/");

  if ( name_position == string::npos ) {
    name_position = 0;
  } else {
    name_position++;
  }

  return string( path, name_position );
}

SerializedFrame::SerializedFrame( const string & path )
  : frame_(),
    source_hash_( get_name( path ) ),
    target_hash_( get_name( path ) ),
    output_raster_()
{
  ifstream file( path, ifstream::binary );
  file.seekg( 0, file.end );
  int size = file.tellg();
  file.seekg( 0, file.beg );

  frame_.resize( size );
  file.read( reinterpret_cast<char *>(frame_.data()), size );
}

SerializedFrame::SerializedFrame( const vector<uint8_t> & frame,
				  const SourceHash & source_hash,
				  const TargetHash & target_hash, 
                                  const Optional<RasterHandle> & output )
  : frame_( frame ),
    source_hash_( source_hash ),
    target_hash_( target_hash ),
    output_raster_( output )
{}

SerializedFrame::SerializedFrame( const Chunk & frame,
				  const SourceHash & source_hash,
				  const TargetHash & target_hash,
                                  const Optional<RasterHandle> & output )
  : SerializedFrame( vector<uint8_t>( frame.buffer(), frame.buffer() + frame.size() ),
		     source_hash, target_hash, output )
{}

Chunk SerializedFrame::chunk( void ) const
{
  return Chunk( &frame_.at( 0 ), frame_.size() );
}

bool SerializedFrame::validate_source( const DecoderHash & decoder_hash ) const
{
  return decoder_hash.can_decode( source_hash_ );
}

bool SerializedFrame::validate_target( const DecoderHash & decoder_hash ) const
{
  // FIXME, could be more rigorous
  return target_hash_.continuation_hash == decoder_hash.continuation_hash();
}

string SerializedFrame::name( void ) const
{
  stringstream name_stream;
  name_stream << hex << uppercase << source_hash_.str() << "#" << target_hash_.str();

  return name_stream.str();
}

double SerializedFrame::psnr( const RasterHandle & original ) const
{
  assert( output_raster_.initialized() );
  return original.get().psnr( output_raster_.get() );
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
