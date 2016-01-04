#include "alfalfa_video_client.hh"
#include "serialization.hh"

using namespace std;

size_t
AlfalfaVideoClient::get_track_size( const size_t track_id ) const
{
  AlfalfaProtobufs::AlfalfaRequest request = get_alfalfa_request_protobuf( AlfalfaRequestType::GetTrackSize );
  *request.mutable_sizet() = SizeT( track_id ).to_protobuf();
  AlfalfaProtobufs::AlfalfaResponse response = server_.handle_request( request );
  if ( not response.has_sizet() )
    throw runtime_error( "Response doesn't have the track size" );
  return SizeT( response.sizet() ).sizet;
}

FrameInfo
AlfalfaVideoClient::get_frame( const size_t frame_id ) const
{
  AlfalfaProtobufs::AlfalfaRequest request = get_alfalfa_request_protobuf( AlfalfaRequestType::GetFrameFrameInfo );
  *request.mutable_sizet() = SizeT( frame_id ).to_protobuf();
  AlfalfaProtobufs::AlfalfaResponse response = server_.handle_request( request );
  if ( not response.has_frame_info() )
    throw runtime_error( "Response doesn't have frame info" );
  return FrameInfo( response.frame_info() );
}

TrackData
AlfalfaVideoClient::get_frame( const size_t track_id, const size_t frame_index ) const
{
  AlfalfaProtobufs::AlfalfaRequest request = get_alfalfa_request_protobuf( AlfalfaRequestType::GetFrameTrackData );
  *request.mutable_track_position() = TrackPosition( track_id, frame_index ).to_protobuf();
  AlfalfaProtobufs::AlfalfaResponse response = server_.handle_request( request );
  if ( not response.has_track_data() )
    throw runtime_error( "Response doesn't have track data" );
  return TrackData( response.track_data() );
}

size_t
AlfalfaVideoClient::get_raster( const size_t raster_index ) const
{
  AlfalfaProtobufs::AlfalfaRequest request = get_alfalfa_request_protobuf( AlfalfaRequestType::GetRaster );
  *request.mutable_sizet() = SizeT( raster_index ).to_protobuf();
  AlfalfaProtobufs::AlfalfaResponse response = server_.handle_request( request );
  if ( not response.has_sizet() )
    throw runtime_error( "Response doesn't have raster hash" );
  return SizeT( response.sizet() ).sizet;
}

vector<FrameInfo>
AlfalfaVideoClient::get_frames( const size_t track_id, const size_t start_frame_index,
                                const size_t end_frame_index ) const
{
  AlfalfaProtobufs::AlfalfaRequest request = get_alfalfa_request_protobuf( AlfalfaRequestType::GetFramesTrack );
  *request.mutable_track_range_args() = TrackRangeArgs( track_id, start_frame_index, end_frame_index ).to_protobuf();
  AlfalfaProtobufs::AlfalfaResponse response = server_.handle_request( request );
  if ( not response.has_frame_iterator() )
    throw runtime_error( "Response doesn't have frame_iterator" );
  return FrameIterator( response.frame_iterator() ).frames;
}

vector<FrameInfo>
AlfalfaVideoClient::get_frames_reverse( const size_t track_id, const size_t start_frame_index, const size_t end_frame_index ) const
{
  AlfalfaProtobufs::AlfalfaRequest request = get_alfalfa_request_protobuf( AlfalfaRequestType::GetFramesReverse );
  *request.mutable_track_range_args() = TrackRangeArgs( track_id, start_frame_index, end_frame_index ).to_protobuf();
  AlfalfaProtobufs::AlfalfaResponse response = server_.handle_request( request );
  if ( not response.has_frame_iterator() )
    throw runtime_error( "Response doesn't have frame_iterator" );
  return FrameIterator( response.frame_iterator() ).frames;
}

vector<FrameInfo>
AlfalfaVideoClient::get_frames( const size_t from_track_id, const size_t to_track_id,
                                const size_t from_frame_index, const size_t switch_start_index,
                                const size_t switch_end_index ) const
{
  AlfalfaProtobufs::AlfalfaRequest request = get_alfalfa_request_protobuf( AlfalfaRequestType::GetFramesSwitch );
  *request.mutable_switch_range_args() = SwitchRangeArgs( from_track_id, to_track_id,
                                                          from_frame_index, switch_start_index,
                                                          switch_end_index ).to_protobuf();
  AlfalfaProtobufs::AlfalfaResponse response = server_.handle_request( request );
  if ( not response.has_frame_iterator() )
    throw runtime_error( "Response doesn't have frame_iterator" );
  return FrameIterator( response.frame_iterator() ).frames;
}

vector<QualityData>
AlfalfaVideoClient::get_quality_data_by_original_raster( const size_t original_raster ) const
{
  AlfalfaProtobufs::AlfalfaRequest request = get_alfalfa_request_protobuf( AlfalfaRequestType::GetQualityDataByOriginalRaster );
  *request.mutable_sizet() = SizeT( original_raster ).to_protobuf();
  AlfalfaProtobufs::AlfalfaResponse response = server_.handle_request( request );
  if ( not response.has_quality_data_iterator() )
    throw runtime_error( "Response doesn't have quality_data_iterator" );
  return QualityDataIterator( response.quality_data_iterator() ).quality_data_items;
}

vector<FrameInfo>
AlfalfaVideoClient::get_frames_by_output_hash( const size_t output_hash ) const
{
  AlfalfaProtobufs::AlfalfaRequest request = get_alfalfa_request_protobuf( AlfalfaRequestType::GetFramesByOutputHash );
  *request.mutable_sizet() = SizeT( output_hash ).to_protobuf();
  AlfalfaProtobufs::AlfalfaResponse response = server_.handle_request( request );
  if ( not response.has_frame_iterator() )
    throw runtime_error( "Response doesn't have frame_iterator" );
  return FrameIterator( response.frame_iterator() ).frames;
}

vector<FrameInfo>
AlfalfaVideoClient::get_frames_by_decoder_hash( const DecoderHash & decoder_hash ) const
{
  AlfalfaProtobufs::AlfalfaRequest request = get_alfalfa_request_protobuf( AlfalfaRequestType::GetFramesByDecoderHash );
  *request.mutable_decoder_hash() = decoder_hash.to_protobuf();
  AlfalfaProtobufs::AlfalfaResponse response = server_.handle_request( request );
  if ( not response.has_frame_iterator() )
    throw runtime_error( "Response doesn't have frame_iterator" );
  return FrameIterator( response.frame_iterator() ).frames;
}

vector<size_t>
AlfalfaVideoClient::get_track_ids() const
{
  AlfalfaProtobufs::AlfalfaRequest request = get_alfalfa_request_protobuf( AlfalfaRequestType::GetTrackIds );
  AlfalfaProtobufs::AlfalfaResponse response = server_.handle_request( request );
  if ( not response.has_track_ids() )
    throw runtime_error( "Response doesn't have track_ids" );
  return TrackIdsIterator( response.track_ids() ).track_ids;
}

vector<TrackData>
AlfalfaVideoClient::get_track_data_by_frame_id( const size_t frame_id ) const
{
  AlfalfaProtobufs::AlfalfaRequest request = get_alfalfa_request_protobuf( AlfalfaRequestType::GetTrackDataByFrameId );
  *request.mutable_sizet() = SizeT( frame_id ).to_protobuf();
  AlfalfaProtobufs::AlfalfaResponse response = server_.handle_request( request );
  if ( not response.has_track_data_iterator() )
    throw runtime_error( "Response doesn't have track_data_iterator" );
  return TrackDataIterator( response.track_data_iterator() ).track_data_items;
}

vector<SwitchInfo>
AlfalfaVideoClient::get_switches_ending_with_frame( const size_t frame_id ) const
{
  AlfalfaProtobufs::AlfalfaRequest request = get_alfalfa_request_protobuf( AlfalfaRequestType::GetSwitchesEndingWithFrame );
  *request.mutable_sizet() = SizeT( frame_id ).to_protobuf();
  AlfalfaProtobufs::AlfalfaResponse response = server_.handle_request( request );
  if ( not response.has_switches() )
    throw runtime_error( "Response doesn't have switches" );
  return Switches( response.switches() ).switches;
}

const Chunk
AlfalfaVideoClient::get_chunk( const FrameInfo & frame_info )
{
  AlfalfaProtobufs::AlfalfaRequest request = get_alfalfa_request_protobuf( AlfalfaRequestType::GetChunk );
  *request.mutable_frame_info() = frame_info.to_protobuf();
  AlfalfaProtobufs::AlfalfaResponse response = server_.handle_request( request );
  if ( not response.has_chunk() )
    throw runtime_error( "Response doesn't have chunk" );
  buffered_chunk_ = response.chunk();
  return Chunk( buffered_chunk_ );
}

size_t
AlfalfaVideoClient::get_video_width() const
{
  AlfalfaProtobufs::AlfalfaRequest request = get_alfalfa_request_protobuf( AlfalfaRequestType::GetVideoWidth );
  AlfalfaProtobufs::AlfalfaResponse response = server_.handle_request( request );
  if ( not response.has_sizet() )
    throw runtime_error( "Response doesn't have video width" );
  return SizeT( response.sizet() ).sizet;
}

size_t
AlfalfaVideoClient::get_video_height() const
{
  AlfalfaProtobufs::AlfalfaRequest request = get_alfalfa_request_protobuf( AlfalfaRequestType::GetVideoHeight );
  AlfalfaProtobufs::AlfalfaResponse response = server_.handle_request( request );
  if ( not response.has_sizet() )
    throw runtime_error( "Response doesn't have video height" );
  return SizeT( response.sizet() ).sizet;
}
