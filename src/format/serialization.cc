#include <fcntl.h>

#include "serialization.hh"

using namespace std;

VideoInfo::VideoInfo()
  : fourcc( "VP80" ),
    width( 0 ),
    height( 0 )
{}

VideoInfo::VideoInfo( string fourcc, uint16_t width, uint16_t height )
  : fourcc( fourcc ),
    width( width ),
    height( height )
{}

VideoInfo::VideoInfo( const AlfalfaProtobufs::VideoInfo & message )
  : fourcc( message.fourcc() ),
    width( message.width() ),
    height( message.height() )
{}

AlfalfaProtobufs::VideoInfo
VideoInfo::to_protobuf() const
{
  AlfalfaProtobufs::VideoInfo message;

  message.set_fourcc( fourcc );
  message.set_width( width );
  message.set_height( height );

  return message;
}

RasterData::RasterData( const size_t hash )
  : hash( hash )
{}

RasterData::RasterData( const AlfalfaProtobufs::RasterData & message )
  : hash( message.hash() )
{}

AlfalfaProtobufs::RasterData
RasterData::to_protobuf() const
{
  AlfalfaProtobufs::RasterData item;
  item.set_hash( hash );
  return item;
}

QualityData::QualityData( const size_t original_raster, const size_t approximate_raster,
                          const double quality )
  : original_raster( original_raster ),
    approximate_raster( approximate_raster ),
    quality( quality )
{}

QualityData::QualityData( const AlfalfaProtobufs::QualityData & message )
  : original_raster( message.original_raster() ),
    approximate_raster( message.approximate_raster() ),
    quality( message.quality() )
{}

AlfalfaProtobufs::QualityData
QualityData::to_protobuf() const
{
  AlfalfaProtobufs::QualityData message;

  message.set_original_raster( original_raster );
  message.set_approximate_raster( approximate_raster );
  message.set_quality( quality );

  return message;
}

TrackData::TrackData( const size_t track_id, const size_t frame_index,
                      const size_t frame_id )
  : track_id( track_id ),
    frame_index( frame_index ),
    frame_id( frame_id )
{}

TrackData::TrackData( const size_t track_id, const size_t frame_id )
  : track_id( track_id ),
    frame_index( 0 ),
    frame_id( frame_id )
{}

TrackData::TrackData( const AlfalfaProtobufs::TrackData & message )
  : track_id( message.track_id() ),
    frame_index( message.frame_index() ),
    frame_id( message.frame_id() )
{}

AlfalfaProtobufs::TrackData
TrackData::to_protobuf() const
{
  AlfalfaProtobufs::TrackData message;

  message.set_track_id( track_id );
  message.set_frame_index( frame_index );
  message.set_frame_id( frame_id );

  return message;
}

SwitchData::SwitchData( const size_t from_track_id, const size_t from_frame_index,
                        const size_t to_track_id, const size_t to_frame_index,
                        const size_t frame_id, const size_t switch_frame_index )
  : from_track_id( from_track_id ),
    from_frame_index( from_frame_index ),
    to_track_id( to_track_id ),
    to_frame_index( to_frame_index ),
    frame_id( frame_id ),
    switch_frame_index( switch_frame_index )
{}

SwitchData::SwitchData( const AlfalfaProtobufs::SwitchData & message )
  : from_track_id( message.from_track_id() ),
    from_frame_index( message.from_frame_index() ),
    to_track_id( message.to_track_id() ),
    to_frame_index( message.to_frame_index() ),
    frame_id( message.frame_id() ),
    switch_frame_index( message.switch_frame_index() )
{}

AlfalfaProtobufs::SwitchData
SwitchData::to_protobuf() const
{
  AlfalfaProtobufs::SwitchData message;

  message.set_from_track_id( from_track_id );
  message.set_from_frame_index( from_frame_index );
  message.set_to_track_id( to_track_id );
  message.set_to_frame_index( to_frame_index );
  message.set_frame_id( frame_id );
  message.set_switch_frame_index( switch_frame_index );

  return message;
}

SourceHash::SourceHash( const AlfalfaProtobufs::SourceHash & message )
  : state_hash( make_optional( message.has_state_hash(), message.state_hash() ) ),
    last_hash( make_optional( message.has_last_hash(), message.last_hash() ) ),
    golden_hash( make_optional( message.has_golden_hash(), message.golden_hash() ) ),
    alt_hash( make_optional( message.has_alt_hash(), message.alt_hash() ) )
{}

AlfalfaProtobufs::SourceHash
SourceHash::to_protobuf() const
{
  AlfalfaProtobufs::SourceHash message;

  if ( state_hash.initialized() ) {
    message.set_state_hash( state_hash.get() );
  }
  else {
    message.clear_state_hash();
  }

  if ( last_hash.initialized() ) {
    message.set_last_hash( last_hash.get() );
  }
  else {
    message.clear_last_hash();
  }

  if ( golden_hash.initialized() ) {
    message.set_golden_hash( golden_hash.get() );
  }
  else {
    message.clear_golden_hash();
  }

  if ( alt_hash.initialized() ) {
    message.set_alt_hash( alt_hash.get() );
  }
  else {
    message.clear_alt_hash();
  }

  return message;
}

TargetHash::TargetHash( const AlfalfaProtobufs::TargetHash & message )
  : UpdateTracker( message.update_last(),
                   message.update_golden(),
                   message.update_alternate(),
                   message.last_to_golden(),
                   message.last_to_alternate(),
                   message.golden_to_alternate(),
                   message.alternate_to_golden() ),
    state_hash( message.state_hash() ),
    output_hash( message.output_hash() ),
    shown( message.shown() )
{}

AlfalfaProtobufs::TargetHash
TargetHash::to_protobuf() const
{
  AlfalfaProtobufs::TargetHash message;

  message.set_state_hash( state_hash );
  message.set_output_hash( output_hash );
  message.set_update_last( update_last );
  message.set_update_golden( update_golden );
  message.set_update_alternate( update_alternate );
  message.set_last_to_golden( last_to_golden );
  message.set_last_to_alternate( last_to_alternate );
  message.set_golden_to_alternate( golden_to_alternate );
  message.set_alternate_to_golden( alternate_to_golden );
  message.set_shown( shown );

  return message;
}

FrameInfo::FrameInfo( const AlfalfaProtobufs::FrameInfo & message )
  : offset_( message.offset() ),
    length_( message.length() ),
    name_( SourceHash( message.source_hash() ),
           TargetHash( message.target_hash() ) ),
    frame_id_( message.frame_id() )
{}

AlfalfaProtobufs::FrameInfo
FrameInfo::to_protobuf() const
{
  AlfalfaProtobufs::FrameInfo message;

  message.set_offset( offset_ );
  message.set_length( length_ );

  *message.mutable_source_hash() = name_.source.to_protobuf();
  *message.mutable_target_hash() = name_.target.to_protobuf();

  message.set_frame_id( frame_id_ );

  return message;
}

ProtobufSerializer::ProtobufSerializer( const string & filename )
  : fout_( SystemCall( filename,
		       open( filename.c_str(),
			     O_WRONLY | O_CREAT | O_EXCL,
			     S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH ) ) )
{}

void ProtobufSerializer::write_string( const string & str )
{
  coded_output_.WriteRaw( str.data(), str.size() );
}

ProtobufDeserializer::ProtobufDeserializer( const string & filename )
  : fin_( SystemCall( filename,
		      open( filename.c_str(), O_RDONLY, 0 ) ) )
{}

string ProtobufDeserializer::read_string( const size_t size )
{
  string ret;
  if ( not coded_input_.ReadString( &ret, size ) ) {
    throw runtime_error( "ReadString returned false" );
  }
  return ret;
}
