#include "frame_info.hh"
#include "file.hh"

using namespace std;

FrameName::FrameName( const string & name )
  : source( name ),
    target( name )
{}

FrameName::FrameName( const SourceHash & s, const TargetHash & t )
  : source( s ),
    target( t )
{}

SerializedFrame::SerializedFrame( const FrameName & name,
                                  const std::vector<uint8_t> & data )
  : name_( name ),
    serialized_frame_( data )
{}

const Chunk SerializedFrame::chunk() const
{
  return Chunk( serialized_frame_.data(), serialized_frame_.size() );
}

string FrameName::str() const
{
  return source.str() + "#" + target.str();
}

FrameInfo::FrameInfo( const FrameName & name, const size_t & offset,
                      const size_t & length )
  : offset_( offset ),
    length_( length ),
    name_( name ),
    frame_id_( 0 )
{}

bool FrameInfo::validate_source( const DecoderHash & decoder_hash ) const
{
  return decoder_hash.can_decode( name_.source );
}

bool FrameInfo::validate_target( const DecoderHash & decoder_hash ) const
{
  /* FIXME this is pointless */
  return name_.target.state_hash == decoder_hash.state_hash();
}

bool FrameInfo::shown() const
{
  return name_.target.shown;
}
