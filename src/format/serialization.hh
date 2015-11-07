#ifndef SERIALIZATION_HH
#define SERIALIZATION_HH

#include <vector>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "optional.hh"
#include "frame_info.hh"
#include "dependency_tracking.hh"
#include "protobufs/alfalfa.pb.h"

struct VideoInfo
{
  std::string fourcc;
  uint16_t width;
  uint16_t height;
  uint32_t frame_rate_numerator;
  uint32_t frame_rate_denominator;
  uint32_t frame_count;

  VideoInfo()
    : fourcc( "VP80" ), width( 0 ), height( 0 ), frame_rate_numerator( 0 ), frame_rate_denominator( 1 ),
      frame_count( 0 )
  {}

  VideoInfo( std::string fourcc, uint16_t width, uint16_t height, uint32_t frame_rate_numerator,
    uint32_t frame_rate_denominator, uint32_t frame_count )
    : fourcc( fourcc ), width( width ), height( height ), frame_rate_numerator( frame_rate_numerator ),
      frame_rate_denominator( frame_rate_denominator ), frame_count( frame_count )
  {}

  double frame_rate() const
  {
      return (double)frame_rate_numerator / frame_rate_denominator;
  }
};

struct RasterData
{
  size_t hash;
};

struct QualityData
{
  size_t original_raster;
  size_t approximate_raster;
  double quality;
};

struct TrackData
{
  size_t track_id;
  size_t frame_id;
  SourceHash source_hash;
  TargetHash target_hash;

  TrackData( const size_t & track_id, const size_t & frame_id,
    const SourceHash & source_hash, const TargetHash & target_hash )
    : track_id( track_id ),
      frame_id( frame_id ),
      source_hash( source_hash ),
      target_hash( target_hash )
  {}

  TrackData()
    : track_id(0), frame_id(0), source_hash(), target_hash()
  {}
};

void to_protobuf( const VideoInfo & info, AlfalfaProtobufs::VideoInfo & message );
void from_protobuf( const AlfalfaProtobufs::VideoInfo & message, VideoInfo & info );
void to_protobuf( const SourceHash & source_hash, AlfalfaProtobufs::SourceHash & message );
void from_protobuf( const AlfalfaProtobufs::SourceHash & message, SourceHash & source_hash );
void to_protobuf( const TargetHash & target_hash, AlfalfaProtobufs::TargetHash & message );
void from_protobuf( const AlfalfaProtobufs::TargetHash & message, TargetHash & target_hash );
void to_protobuf( const RasterData & rd, AlfalfaProtobufs::RasterData & item );
void from_protobuf( const AlfalfaProtobufs::RasterData & item, RasterData & rd );
void to_protobuf( const QualityData & qd, AlfalfaProtobufs::QualityData & message );
void from_protobuf( const AlfalfaProtobufs::QualityData & message, QualityData & qd );
void to_protobuf( const TrackData & td, AlfalfaProtobufs::TrackData & message );
void from_protobuf( const AlfalfaProtobufs::TrackData & message, TrackData & td );
void to_protobuf( const FrameInfo & fi, AlfalfaProtobufs::FrameInfo & message );
void from_protobuf( const AlfalfaProtobufs::FrameInfo & pfi, FrameInfo & fi );

// end of protobuf converters

class ProtobufSerializer
{
protected:
  std::ofstream fout_;
  unique_ptr<google::protobuf::io::ZeroCopyOutputStream> raw_output_;
  unique_ptr<google::protobuf::io::CodedOutputStream> coded_output_;

public:
  ProtobufSerializer( const std::string & filename );
  bool write_raw( const void * raw_data, size_t size );

  template<class EntryProtobufType>
  bool write_protobuf( const EntryProtobufType & entry );

  void close();
  ~ProtobufSerializer();
};

class ProtobufDeserializer
{
protected:
  std::ifstream fin_;
  unique_ptr<google::protobuf::io::ZeroCopyInputStream> raw_input_;
  unique_ptr<google::protobuf::io::CodedInputStream> coded_input_;

public:
  ProtobufDeserializer( const std::string & filename );
  bool read_raw( void * raw_data, size_t size );

  template<class EntryProtobufType>
  bool read_protobuf( EntryProtobufType & protobuf );

  void close();
  ~ProtobufDeserializer();
};

template<class EntryProtobufType>
bool ProtobufDeserializer::read_protobuf( EntryProtobufType & message )
{
  if ( not fin_.is_open() ) {
    return false;
  }

  google::protobuf::uint32 size;
  bool has_next = coded_input_->ReadLittleEndian32( &size );

  if ( not has_next ) {
    return false;
  }

  google::protobuf::io::CodedInputStream::Limit message_limit =
    coded_input_->PushLimit( size );

  if ( message.ParseFromCodedStream( coded_input_.get() ) ) {
    coded_input_->PopLimit( message_limit );
    return true;
  }

  return false;
}

template<class EntryProtobufType>
bool ProtobufSerializer::write_protobuf( const EntryProtobufType & protobuf )
{
  if ( not fout_.is_open() ) {
    return false;
  }

  coded_output_->WriteLittleEndian32( protobuf.ByteSize() );
  protobuf.SerializeToCodedStream( coded_output_.get() );

  return true;
}

#endif /* SERIALIZATION_HH */
