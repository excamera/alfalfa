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

SizeT::SizeT( const size_t sizet )
  : sizet( sizet )
{}

SizeT::SizeT( const AlfalfaProtobufs::SizeT & message )
  : sizet( message.sizet() )
{}

AlfalfaProtobufs::SizeT
SizeT::to_protobuf() const
{
  AlfalfaProtobufs::SizeT message;
  message.set_sizet( sizet );
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

DecoderHash::DecoderHash( const AlfalfaProtobufs::DecoderHash & message )
  : state_hash_( message.state_hash() ),
    last_hash_( message.last_hash() ),
    golden_hash_( message.golden_hash() ),
    alt_hash_( message.alt_hash() )
{}

AlfalfaProtobufs::DecoderHash
DecoderHash::to_protobuf() const
{
  AlfalfaProtobufs::DecoderHash message;

  message.set_state_hash( state_hash_ );
  message.set_last_hash( last_hash_ );
  message.set_golden_hash( golden_hash_ );
  message.set_alt_hash( alt_hash_ );

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

FrameIterator::FrameIterator()
  : frames()
{}

FrameIterator::FrameIterator( const AlfalfaProtobufs::FrameIterator & message )
  : frames()
{
  int i;
  for ( i = 0; i < message.frame_size(); i++ ) {
    frames.push_back( FrameInfo( message.frame( i ) ) );
  }
}

AlfalfaProtobufs::FrameIterator
FrameIterator::to_protobuf() const
{
  AlfalfaProtobufs::FrameIterator message;

  for ( FrameInfo frame : frames ) {
    AlfalfaProtobufs::FrameInfo *fi = message.add_frame();
    *fi = frame.to_protobuf();
  }

  return message;
}

QualityDataIterator::QualityDataIterator()
  : quality_data_items()
{}

QualityDataIterator::QualityDataIterator( const AlfalfaProtobufs::QualityDataIterator & message )
  : quality_data_items()
{
  int i;
  for ( i = 0; i < message.quality_data_size(); i++ ) {
    quality_data_items.push_back( QualityData( message.quality_data( i ) ) );
  }
}

AlfalfaProtobufs::QualityDataIterator
QualityDataIterator::to_protobuf() const
{
  AlfalfaProtobufs::QualityDataIterator message;

  for ( QualityData quality_data_item : quality_data_items ) {
    AlfalfaProtobufs::QualityData *qd = message.add_quality_data();
    *qd = quality_data_item.to_protobuf();
  }

  return message;
}

TrackDataIterator::TrackDataIterator()
  : track_data_items()
{}

TrackDataIterator::TrackDataIterator( const AlfalfaProtobufs::TrackDataIterator & message )
  : track_data_items()
{
  int i;
  for ( i = 0; i < message.track_data_size(); i++ ) {
    track_data_items.push_back( TrackData( message.track_data( i ) ) );
  }
}

AlfalfaProtobufs::TrackDataIterator
TrackDataIterator::to_protobuf() const
{
  AlfalfaProtobufs::TrackDataIterator message;

  for ( TrackData track_data_item : track_data_items ) {
    AlfalfaProtobufs::TrackData *td = message.add_track_data();
    *td = track_data_item.to_protobuf();
  }

  return message;
}

Switches::Switches()
  : switches()
{}

Switches::Switches( const AlfalfaProtobufs::Switches & message )
  : switches()
{
  int i;
  for ( i = 0; i < message.switch_frames_size(); i++ ) {
    switches.push_back( FrameIterator( message.switch_frames( i ) ) );
  }
}

AlfalfaProtobufs::Switches
Switches::to_protobuf() const
{
  AlfalfaProtobufs::Switches message;

  for ( FrameIterator switch_frames : switches ) {
    AlfalfaProtobufs::FrameIterator *fi = message.add_switch_frames();
    *fi = switch_frames.to_protobuf();
  }

  return message;
}

TrackIdsIterator::TrackIdsIterator()
  : track_ids()
{}

TrackIdsIterator::TrackIdsIterator( const AlfalfaProtobufs::TrackIdsIterator & message )
  : track_ids()
{
  int i;
  for ( i = 0; i < message.track_id_size(); i++ ) {
    track_ids.push_back( message.track_id( i ) );
  }
}

AlfalfaProtobufs::TrackIdsIterator
TrackIdsIterator::to_protobuf() const
{
  AlfalfaProtobufs::TrackIdsIterator message;

  for ( size_t track_id : track_ids ) {
    message.add_track_id( track_id );
  }

  return message;
}

TrackPosition::TrackPosition( const size_t track_id, const size_t frame_index )
  : track_id( track_id ),
    frame_index( frame_index )
{}

TrackPosition::TrackPosition( const AlfalfaProtobufs::TrackPosition & message )
  : track_id( message.track_id() ),
    frame_index( message.frame_index() )
{}

AlfalfaProtobufs::TrackPosition
TrackPosition::to_protobuf() const
{
  AlfalfaProtobufs::TrackPosition message;

  message.set_track_id( track_id );
  message.set_frame_index( frame_index );

  return message;
}

TrackRangeArgs::TrackRangeArgs( const size_t track_id, const size_t start_frame_index,
                                const size_t end_frame_index )
  : track_id( track_id ),
    start_frame_index( start_frame_index ),
    end_frame_index( end_frame_index )
{}

TrackRangeArgs::TrackRangeArgs( const AlfalfaProtobufs::TrackRangeArgs & message )
  : track_id( message.track_id() ),
    start_frame_index( message.start_frame_index() ),
    end_frame_index( message.end_frame_index() )
{}

AlfalfaProtobufs::TrackRangeArgs
TrackRangeArgs::to_protobuf() const
{
  AlfalfaProtobufs::TrackRangeArgs message;

  message.set_track_id( track_id );
  message.set_start_frame_index( start_frame_index );
  message.set_end_frame_index( end_frame_index );

  return message;
}

SwitchRangeArgs::SwitchRangeArgs( const size_t from_track_id, const size_t to_track_id,
                                  const size_t from_frame_index, const size_t switch_start_index,
                                  const size_t switch_end_index )
  : from_track_id( from_track_id ),
    to_track_id( to_track_id ),
    from_frame_index( from_frame_index ),
    switch_start_index( switch_start_index ),
    switch_end_index( switch_end_index )
{}

SwitchRangeArgs::SwitchRangeArgs( const AlfalfaProtobufs::SwitchRangeArgs & message )
  : from_track_id( message.from_track_id() ),
    to_track_id( message.to_track_id() ),
    from_frame_index( message.from_frame_index() ),
    switch_start_index( message.switch_start_index() ),
    switch_end_index( message.switch_end_index() )
{}

AlfalfaProtobufs::SwitchRangeArgs
SwitchRangeArgs::to_protobuf() const
{
  AlfalfaProtobufs::SwitchRangeArgs message;

  message.set_from_track_id( message.from_track_id() );
  message.set_to_track_id( message.to_track_id() );
  message.set_from_frame_index( message.from_frame_index() );
  message.set_switch_start_index( message.switch_start_index() );
  message.set_switch_end_index( message.switch_end_index() );

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
