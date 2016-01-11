#ifndef SERIALIZATION_HH
#define SERIALIZATION_HH

#include <vector>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "optional.hh"
#include "frame_info.hh"
#include "dependency_tracking.hh"
#include "alfalfa.pb.h"
#include "file_descriptor.hh"
#include "chunk.hh"

struct VideoInfo
{
  uint16_t width;
  uint16_t height;

  VideoInfo();
  VideoInfo( uint16_t width, uint16_t height );
  VideoInfo( const AlfalfaProtobufs::VideoInfo & message );

  AlfalfaProtobufs::VideoInfo to_protobuf() const;

  bool operator==( const VideoInfo & other ) const
  {
    return ( width == other.width and
	     height == other.height
    );
  }

  bool operator!=( const VideoInfo & other ) const
  {
    return not ( ( *this ) == other );
  }
};

struct RasterData
{
  size_t hash;

  RasterData( const size_t hash );
  RasterData( const AlfalfaProtobufs::RasterData & message );

  AlfalfaProtobufs::RasterData to_protobuf() const;
};

struct QualityData
{
  size_t original_raster;
  size_t approximate_raster;
  double quality;

  QualityData( const size_t original_raster, const size_t approximate_raster,
               const double quality );
  QualityData( const AlfalfaProtobufs::QualityData & message );

  AlfalfaProtobufs::QualityData to_protobuf() const;
};

struct TrackData
{
  size_t track_id;
  size_t frame_index;
  size_t displayed_raster_index;  // required since different tracks can have
                                // different number of frames; this is to ensure that
                                // we can easily line up different tracks according to
                                // displayed raster.
  size_t frame_id;

  TrackData( const size_t track_id, const size_t frame_id );
  TrackData( const AlfalfaProtobufs::TrackData & message );

  AlfalfaProtobufs::TrackData to_protobuf() const;
};

struct SwitchData
{
  size_t from_track_id;
  size_t from_frame_index;
  size_t to_track_id;
  size_t to_frame_index;
  size_t frame_id;
  size_t switch_frame_index;

  SwitchData( const size_t from_track_id, const size_t from_frame_index,
              const size_t to_track_id, const size_t to_frame_index,
              const size_t frame_id, const size_t switch_frame_index );
  SwitchData( const AlfalfaProtobufs::SwitchData & message );

  AlfalfaProtobufs::SwitchData to_protobuf() const;
};

struct SizeT
{
  size_t sizet;

  SizeT( const size_t sizet );
  SizeT( const AlfalfaProtobufs::SizeT & message );

  AlfalfaProtobufs::SizeT to_protobuf() const;
};

struct FrameIterator
{
  std::vector<FrameInfo> frames;

  FrameIterator();
  FrameIterator( const AlfalfaProtobufs::FrameIterator & message );

  AlfalfaProtobufs::FrameIterator to_protobuf() const;
};

struct QualityDataIterator
{
  std::vector<QualityData> quality_data_items;

  QualityDataIterator();
  QualityDataIterator( const AlfalfaProtobufs::QualityDataIterator & message );

  AlfalfaProtobufs::QualityDataIterator to_protobuf() const;
};

struct TrackDataIterator
{
  std::vector<TrackData> track_data_items;

  TrackDataIterator();
  TrackDataIterator( const AlfalfaProtobufs::TrackDataIterator & message );

  AlfalfaProtobufs::TrackDataIterator to_protobuf() const;
};

struct SwitchInfo
{
  std::vector<FrameInfo> frames;
  size_t from_track_id;
  size_t to_track_id;
  size_t from_frame_index;
  size_t to_frame_index;
  size_t switch_start_index;
  size_t switch_end_index;

  SwitchInfo( const size_t from_track_id, const size_t to_track_id, const size_t from_frame_index,
              const size_t to_frame_index, const size_t switch_start_index, const size_t switch_end_index );
  SwitchInfo( const AlfalfaProtobufs::SwitchInfo & message );

  AlfalfaProtobufs::SwitchInfo to_protobuf() const;
};

struct Switches
{
  std::vector<SwitchInfo> switches;

  Switches();
  Switches( const AlfalfaProtobufs::Switches & message );

  AlfalfaProtobufs::Switches to_protobuf() const;
};

struct TrackIdsIterator
{
  std::vector<size_t> track_ids;

  TrackIdsIterator();
  TrackIdsIterator( const AlfalfaProtobufs::TrackIdsIterator & message );

  AlfalfaProtobufs::TrackIdsIterator to_protobuf() const;
};

struct TrackPosition
{
  size_t track_id;
  size_t frame_index;

  TrackPosition( const size_t track_id, const size_t frame_index );
  TrackPosition( const AlfalfaProtobufs::TrackPosition & message );

  AlfalfaProtobufs::TrackPosition to_protobuf() const;
};

struct TrackRangeArgs
{
  size_t track_id;
  size_t start_frame_index;
  size_t end_frame_index;

  TrackRangeArgs( const size_t track_id, const size_t start_frame_index, const size_t end_frame_index );
  TrackRangeArgs( const AlfalfaProtobufs::TrackRangeArgs & message );

  AlfalfaProtobufs::TrackRangeArgs to_protobuf() const;
};

struct SwitchRangeArgs
{
  size_t from_track_id;
  size_t to_track_id;
  size_t from_frame_index;
  size_t switch_start_index;
  size_t switch_end_index;

  SwitchRangeArgs( const size_t from_track_id, const size_t to_track_id,
                   const size_t from_frame_index, const size_t switch_start_index,
                   const size_t switch_end_index );
  SwitchRangeArgs( const AlfalfaProtobufs::SwitchRangeArgs & message );

  AlfalfaProtobufs::SwitchRangeArgs to_protobuf() const;
};

class ProtobufSerializer
{
protected:
  FileDescriptor fout_;
  google::protobuf::io::FileOutputStream raw_output_ { fout_.num() };
  google::protobuf::io::CodedOutputStream coded_output_ { &raw_output_ };

public:
  ProtobufSerializer( const std::string & filename );
  ProtobufSerializer( FileDescriptor && fd );
  void write_string( const std::string & str );

  template<class EntryProtobufType>
  void write_protobuf( const EntryProtobufType & entry );
};

class ProtobufDeserializer
{
protected:
  FileDescriptor fin_;
  google::protobuf::io::FileInputStream raw_input_ { fin_.num() };
  google::protobuf::io::CodedInputStream coded_input_ { &raw_input_ };

public:
  ProtobufDeserializer( const std::string & filename );
  ProtobufDeserializer( FileDescriptor && fd );
  std::string read_string( const size_t size );

  template<class EntryProtobufType>
  bool read_protobuf( EntryProtobufType & protobuf );
};

template<class EntryProtobufType>
bool ProtobufDeserializer::read_protobuf( EntryProtobufType & message )
{
  google::protobuf::uint32 size;
  bool has_next = coded_input_.ReadLittleEndian32( &size );

  if ( not has_next ) {
    return false;
  }

  google::protobuf::io::CodedInputStream::Limit message_limit =
    coded_input_.PushLimit( size );

  if ( message.ParseFromCodedStream( &coded_input_ ) ) {
    coded_input_.PopLimit( message_limit );
    return true;
  }

  return false;
}

template<class EntryProtobufType>
void ProtobufSerializer::write_protobuf( const EntryProtobufType & protobuf )
{
  coded_output_.WriteLittleEndian32( protobuf.ByteSize() );
  if ( not protobuf.SerializeToCodedStream( &coded_output_ ) ) {
    throw std::runtime_error( "write_protobuf: write error" );
  }
}

#endif /* SERIALIZATION_HH */
