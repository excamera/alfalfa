#include "frame_info.hh"
#include "file.hh"

using namespace std;

FrameInfo::FrameInfo( const string & frame_name,
                      const size_t & offset, const size_t & length )
  : offset_( offset ),
    length_( length ),
    index_( 0 ),
    source_hash_( frame_name ),
    target_hash_( frame_name ),
    frame_id_( 0 )
{}

FrameInfo::FrameInfo( const size_t & offset, const size_t & length,
                      const SourceHash & source_hash, const TargetHash & target_hash )
  : offset_( offset ),
    length_( length ),
    index_( 0 ),
    source_hash_( source_hash ),
    target_hash_( target_hash ),
    frame_id_( 0 )
{}

bool FrameInfo::validate_source( const DecoderHash & decoder_hash ) const
{
  return decoder_hash.can_decode( source_hash_ );
}

bool FrameInfo::validate_target( const DecoderHash & decoder_hash ) const
{
  return target_hash_.continuation_hash == decoder_hash.continuation_hash();
}

bool FrameInfo::shown() const
{
  return target_hash_.shown;
}

string build_frame_name( const SourceHash & source_hash,
  const TargetHash & target_hash )
{
  return source_hash.str() + "#" + target_hash.str();
}
