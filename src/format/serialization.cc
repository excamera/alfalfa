#include "serialization.hh"

void to_protobuf( const SourceHash & source_hash,
  AlfalfaProtobufs::SourceHash & message )
{
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
}

void from_protobuf( const AlfalfaProtobufs::SourceHash & message,
  SourceHash & source_hash )
{
  source_hash.state_hash = make_optional( message.has_state_hash(),
    message.state_hash() );
  source_hash.continuation_hash = make_optional( message.has_continuation_hash(),
    message.continuation_hash() );
  source_hash.last_hash = make_optional( message.has_last_hash(),
    message.last_hash() );
  source_hash.golden_hash = make_optional( message.has_golden_hash(),
    message.golden_hash() );
  source_hash.alt_hash = make_optional( message.has_alt_hash(),
    message.alt_hash() );
}

void to_protobuf( const TargetHash & target_hash,
  AlfalfaProtobufs::TargetHash & message )
{
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
}

void from_protobuf( const AlfalfaProtobufs::TargetHash & message,
  TargetHash & target_hash )
{
  target_hash.update_last = message.update_last();
  target_hash.update_golden = message.update_golden();
  target_hash.update_alternate = message.update_alternate();
  target_hash.last_to_golden = message.last_to_golden();
  target_hash.last_to_alternate = message.last_to_alternate();
  target_hash.golden_to_alternate = message.golden_to_alternate();
  target_hash.alternate_to_golden = message.alternate_to_golden();

  target_hash.state_hash = message.state_hash();
  target_hash.continuation_hash = message.continuation_hash();
  target_hash.output_hash = message.output_hash();
  target_hash.shown = message.shown();
}

void to_protobuf( const RasterData & rd, AlfalfaProtobufs::RasterData & item )
{
  item.set_hash( rd.hash );
}

void from_protobuf( const AlfalfaProtobufs::RasterData & item, RasterData & rd )
{
  rd.hash = item.hash();
}

void to_protobuf( const QualityData & qd,
  AlfalfaProtobufs::QualityData & message )
{
  message.set_original_raster( qd.original_raster );
  message.set_approximate_raster( qd.approximate_raster );
  message.set_quality( qd.quality );
}

void from_protobuf( const AlfalfaProtobufs::QualityData & message,
  QualityData & qd )
{
  qd.original_raster = message.original_raster();
  qd.approximate_raster = message.approximate_raster();
  qd.quality = message.quality();
}

void to_protobuf( const TrackData & td,
  AlfalfaProtobufs::TrackData & message ) {
  message.set_track_id( td.track_id );
  message.set_frame_id( td.frame_id );
  to_protobuf( td.source_hash, *message.mutable_source_hash() );
  to_protobuf( td.target_hash, *message.mutable_target_hash() );
}

void from_protobuf( const AlfalfaProtobufs::TrackData & message,
  TrackData & td )
{
  td.track_id = message.track_id();
  td.frame_id = message.frame_id();
  from_protobuf( message.source_hash(), td.source_hash );
  from_protobuf( message.target_hash(), td.target_hash );
}

void to_protobuf( const FrameInfo & fi, AlfalfaProtobufs::FrameInfo & message )
{
  message.set_offset( fi.offset() );
  message.set_length( fi.length() );

  if ( fi.ivf_filename().initialized() ) {
    message.set_ivf_filename( fi.ivf_filename().get() );
  }

  to_protobuf( fi.source_hash(), *message.mutable_source_hash() );
  to_protobuf( fi.target_hash(), *message.mutable_target_hash() );
}

void from_protobuf( const AlfalfaProtobufs::FrameInfo & pfi, FrameInfo & fi )
{
  SourceHash source_hash;
  TargetHash target_hash;

  from_protobuf( pfi.source_hash(), source_hash );
  from_protobuf( pfi.target_hash(), target_hash );

  fi.set_source_hash( source_hash );
  fi.set_target_hash( target_hash );
  fi.set_offset( size_t( pfi.offset() ) );
  fi.set_length( size_t( pfi.length() ) );
  fi.set_ivf_filename( pfi.ivf_filename() );
}

ProtobufSerializer::ProtobufSerializer( const std::string & filename )
: fout_( filename ), raw_output_( nullptr ), coded_output_( nullptr )
{
  raw_output_.reset(
    new google::protobuf::io::OstreamOutputStream( &fout_ ) );
  coded_output_.reset(
    new google::protobuf::io::CodedOutputStream( raw_output_.get() ) );
}

bool ProtobufSerializer::write_raw( const void * raw_data, size_t size )
{
  if ( not fout_.is_open() ) {
    return false;
  }

  coded_output_->WriteRaw( raw_data, size );
  return true;
}

void ProtobufSerializer::close()
{
  coded_output_.reset( nullptr );
  raw_output_.reset( nullptr );

  if ( fout_.is_open() ) {
    fout_.close();
  }
}

ProtobufSerializer::~ProtobufSerializer()
{
  close();
}

ProtobufDeserializer::ProtobufDeserializer( const std::string & filename )
:fin_( filename ), raw_input_( nullptr ), coded_input_( nullptr )
{
  raw_input_.reset(
    new google::protobuf::io::IstreamInputStream( &fin_ ) );
  coded_input_.reset(
    new google::protobuf::io::CodedInputStream( raw_input_.get() ) );
}

bool ProtobufDeserializer::read_raw( void * raw_data, size_t size )
{
  return coded_input_->ReadRaw( raw_data, size );
}

void ProtobufDeserializer::close()
{
  coded_input_.reset( nullptr );
  raw_input_.reset( nullptr );

  if ( fin_.is_open() ) {
    fin_.close();
  }
}

ProtobufDeserializer::~ProtobufDeserializer()
{
  close();
}
