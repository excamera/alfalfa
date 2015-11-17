#include <fcntl.h>

#include "serialization.hh"

using namespace std;

AlfalfaProtobufs::VideoInfo to_protobuf( const VideoInfo & info )
{
  AlfalfaProtobufs::VideoInfo message;

  message.set_fourcc( info.fourcc );
  message.set_width( info.width );
  message.set_height( info.height );

  return message;
}

VideoInfo from_protobuf( const AlfalfaProtobufs::VideoInfo & message )
{
  VideoInfo info;

  info.fourcc = message.fourcc();
  info.width = message.width();
  info.height = message.height();

  return info;
}

AlfalfaProtobufs::SourceHash to_protobuf( const SourceHash & source_hash )
{
  AlfalfaProtobufs::SourceHash message;

  if ( source_hash.state_hash.initialized() ) {
    message.set_state_hash( source_hash.state_hash.get() );
  }
  else {
    message.clear_state_hash();
  }

  if ( source_hash.continuation_hash.initialized() ) {
    message.set_continuation_hash( source_hash.continuation_hash.get() );
  }
  else {
    message.clear_continuation_hash();
  }

  if ( source_hash.last_hash.initialized() ) {
    message.set_last_hash( source_hash.last_hash.get() );
  }
  else {
    message.clear_last_hash();
  }

  if ( source_hash.golden_hash.initialized() ) {
    message.set_golden_hash( source_hash.golden_hash.get() );
  }
  else {
    message.clear_golden_hash();
  }

  if ( source_hash.alt_hash.initialized() ) {
    message.set_alt_hash( source_hash.alt_hash.get() );
  }
  else {
    message.clear_alt_hash();
  }

  return message;
}

SourceHash from_protobuf( const AlfalfaProtobufs::SourceHash & message )
{
  return SourceHash(
    make_optional( message.has_state_hash(), message.state_hash() ),
    make_optional( message.has_continuation_hash(), message.continuation_hash() ),
    make_optional( message.has_last_hash(), message.last_hash() ),
    make_optional( message.has_golden_hash(), message.golden_hash() ),
    make_optional( message.has_alt_hash(), message.alt_hash() )
  );
}

AlfalfaProtobufs::TargetHash to_protobuf( const TargetHash & target_hash )
{
  AlfalfaProtobufs::TargetHash message;

  message.set_state_hash( target_hash.state_hash );
  message.set_continuation_hash( target_hash.continuation_hash );
  message.set_output_hash( target_hash.output_hash );
  message.set_update_last( target_hash.update_last );
  message.set_update_golden( target_hash.update_golden );
  message.set_update_alternate( target_hash.update_alternate );
  message.set_last_to_golden( target_hash.last_to_golden );
  message.set_last_to_alternate( target_hash.last_to_alternate );
  message.set_golden_to_alternate( target_hash.golden_to_alternate );
  message.set_alternate_to_golden( target_hash.alternate_to_golden );
  message.set_shown( target_hash.shown );

  return message;
}

TargetHash from_protobuf( const AlfalfaProtobufs::TargetHash & message )
{
  UpdateTracker update_tracker(
    message.update_last(), message.update_golden(), message.update_alternate(),
    message.last_to_golden(), message.last_to_alternate(),
    message.golden_to_alternate(), message.alternate_to_golden()
  );

  return TargetHash(
    update_tracker, message.state_hash(), message.continuation_hash(),
    message.output_hash(), message.shown()
  );
}

AlfalfaProtobufs::RasterData to_protobuf( const RasterData & rd )
{
  AlfalfaProtobufs::RasterData item;
  item.set_hash( rd.hash );
  return item;
}

RasterData from_protobuf( const AlfalfaProtobufs::RasterData & item )
{
  return RasterData{ item.hash() };
}

AlfalfaProtobufs::QualityData to_protobuf( const QualityData & qd )
{
  AlfalfaProtobufs::QualityData message;

  message.set_original_raster( qd.original_raster );
  message.set_approximate_raster( qd.approximate_raster );
  message.set_quality( qd.quality );

  return message;
}

QualityData from_protobuf( const AlfalfaProtobufs::QualityData & message )
{
  QualityData qd;

  qd.original_raster = message.original_raster();
  qd.approximate_raster = message.approximate_raster();
  qd.quality = message.quality();

  return qd;
}

AlfalfaProtobufs::TrackData to_protobuf( const TrackData & td )
{
  AlfalfaProtobufs::TrackData message;

  message.set_track_id( td.track_id );
  message.set_frame_index( td.frame_index );
  message.set_frame_id( td.frame_id );

  return message;
}

TrackData from_protobuf( const AlfalfaProtobufs::TrackData & message )
{
  return TrackData{
    message.track_id(),
    message.frame_index(),
    message.frame_id()
  };
}

AlfalfaProtobufs::FrameInfo to_protobuf( const FrameInfo & fi )
{
  AlfalfaProtobufs::FrameInfo message;

  message.set_offset( fi.offset() );
  message.set_length( fi.length() );

  *message.mutable_source_hash() = to_protobuf( fi.source_hash() );
  *message.mutable_target_hash() = to_protobuf( fi.target_hash() );

  message.set_frame_id( fi.frame_id() );

  return message;
}

FrameInfo from_protobuf( const AlfalfaProtobufs::FrameInfo & pfi )
{
  SourceHash source_hash = from_protobuf( pfi.source_hash() );
  TargetHash target_hash = from_protobuf( pfi.target_hash() );

  FrameInfo fi(
    size_t( pfi.offset() ), size_t( pfi.length() ), source_hash, target_hash
  );

  fi.set_frame_id( size_t( pfi.frame_id() ) );
  return fi;
}

AlfalfaProtobufs::SwitchData to_protobuf( const SwitchData & sd )
{
  AlfalfaProtobufs::SwitchData message;

  message.set_from_track_id( sd.from_track_id );
  message.set_from_frame_index( sd.from_frame_index );
  message.set_to_track_id( sd.to_track_id );
  message.set_to_frame_index( sd.to_frame_index );
  message.set_frame_id( sd.frame_id );
  message.set_switch_frame_index( sd.switch_frame_index );

  return message;
}

SwitchData from_protobuf( const AlfalfaProtobufs::SwitchData & psd )
{
  return SwitchData{
    psd.from_track_id(), psd.from_frame_index(), psd.to_track_id(),
    psd.to_frame_index(), psd.frame_id(), psd.switch_frame_index()
  };
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
