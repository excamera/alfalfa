#include "alfalfa_video_server.hh"
#include "serialization.hh"

using namespace std;

SizeT
AlfalfaVideoServer::get_track_size( const SizeT & track_id ) const
{
  SizeT track_id_deserialized( track_id );
  return SizeT( video_.get_track_size( track_id_deserialized.sizet ) );
}

FrameInfo
AlfalfaVideoServer::get_frame( const SizeT & frame_id ) const
{
  SizeT frame_id_deserialized( frame_id );
  return video_.get_frame( frame_id_deserialized.sizet );
}

TrackData
AlfalfaVideoServer::get_frame( const TrackPosition & track_pos ) const
{
  TrackPosition track_pos_deserialized( track_pos );
  return video_.get_frame( track_pos_deserialized.track_id, track_pos_deserialized.frame_index );
}

SizeT
AlfalfaVideoServer::get_raster( const SizeT & raster_index ) const
{
  SizeT raster_index_deserialized( raster_index );
  return SizeT( video_.get_raster( raster_index_deserialized.sizet ).hash );
}

FrameIterator
AlfalfaVideoServer::get_frames( const TrackRangeArgs & args ) const
{
  TrackRangeArgs args_deserialized( args );
  auto frames = video_.get_track_range( args_deserialized.track_id,
                                        args_deserialized.start_frame_index,
                                        args_deserialized.end_frame_index );
  FrameIterator frame_iterator;
  while ( frames.first != frames.second ) {
    frame_iterator.frames.push_back( *frames.first );
    frames.first++;
  }
  return frame_iterator;
}

FrameIterator
AlfalfaVideoServer::get_frames_reverse( const TrackRangeArgs & args ) const
{
  TrackRangeArgs args_deserialized( args );
  auto frames = video_.get_frames_reverse( args_deserialized.track_id,
                                           args_deserialized.start_frame_index,
                                           args_deserialized.end_frame_index );
  FrameIterator frame_iterator;
  while ( frames.first != frames.second ) {
    frame_iterator.frames.push_back( *frames.first );
    frames.first--;
  }
  return frame_iterator;
}

FrameIterator
AlfalfaVideoServer::get_frames( const SwitchRangeArgs & args ) const
{
  SwitchRangeArgs args_deserialized( args );
  auto frames = video_.get_switch_range( args_deserialized.from_track_id,
                                         args_deserialized.to_track_id,
                                         args_deserialized.from_frame_index,
                                         args_deserialized.switch_start_index,
                                         args_deserialized.switch_end_index );

  FrameIterator frame_iterator;
  while ( frames.first != frames.second ) {
    frame_iterator.frames.push_back( *frames.first );
    frames.first++;
  }
  return frame_iterator;
}

QualityDataIterator
AlfalfaVideoServer::get_quality_data_by_original_raster( const SizeT & original_raster ) const
{
  SizeT original_raster_deserialized( original_raster );
  auto quality_data = video_.get_quality_data_by_original_raster( original_raster_deserialized.sizet );
  QualityDataIterator quality_data_iterator;
  while ( quality_data.first != quality_data.second ) {
    quality_data_iterator.quality_data_items.push_back( *quality_data.first );
    quality_data.first++;
  }
  return quality_data_iterator;
}

FrameIterator
AlfalfaVideoServer::get_frames_by_output_hash( const SizeT & output_hash ) const
{
  SizeT output_hash_deserialized( output_hash );
  auto frames = video_.get_frames_by_output_hash( output_hash_deserialized.sizet );
  FrameIterator frame_iterator;
  while ( frames.first != frames.second ) {
    frame_iterator.frames.push_back( *frames.first );
    frames.first++;
  }
  return frame_iterator;
}

FrameIterator
AlfalfaVideoServer::get_frames_by_decoder_hash( const DecoderHash & decoder_hash ) const
{
  DecoderHash decoder_hash_deserialized( decoder_hash );
  auto frames = video_.get_frames_by_decoder_hash( decoder_hash );
  FrameIterator frame_iterator;
  while ( frames.first != frames.second ) {
    frame_iterator.frames.push_back( *frames.first );
    frames.first++;
  }
  return frame_iterator;
}

TrackIdsIterator
AlfalfaVideoServer::get_track_ids() const
{
  auto track_ids = video_.get_track_ids();
  TrackIdsIterator track_ids_iterator;
  while ( track_ids.first != track_ids.second ) {
    track_ids_iterator.track_ids.push_back( *track_ids.first );
    track_ids.first++;
  }
  return track_ids_iterator;
}

TrackDataIterator
AlfalfaVideoServer::get_track_data_by_frame_id( const SizeT & frame_id ) const
{
  SizeT frame_id_deserialized( frame_id );
  auto track_data = video_.get_track_data_by_frame_id( frame_id_deserialized.sizet );
  TrackDataIterator track_data_iterator;
  while ( track_data.first != track_data.second ) {
    track_data_iterator.track_data_items.push_back( *track_data.first );
    track_data.first++;
  }
  return track_data_iterator;
}

Switches
AlfalfaVideoServer::get_switches_ending_with_frame( const SizeT & frame_id ) const
{
  SizeT frame_id_deserialized( frame_id );
  auto switches = video_.get_switches_ending_with_frame( frame_id_deserialized.sizet );
  Switches sw;
  for ( auto frames : switches ) {
    SwitchInfo switch_info( frames.first.from_track_id(), frames.first.to_track_id(),
                            frames.first.from_frame_index(), frames.first.to_frame_index(),
                            frames.first.switch_frame_index(),
                            frames.second.switch_frame_index() );
    while ( frames.first != frames.second ) {
      switch_info.frames.push_back( *frames.first );
      frames.first--;
    }
    sw.switches.push_back( switch_info );
  }
  return sw;
}

Chunk
AlfalfaVideoServer::get_chunk( const FrameInfo & frame_info ) const
{
  FrameInfo frame_info_deserialized( frame_info );
  return video_.get_chunk( frame_info_deserialized );
}

SizeT
AlfalfaVideoServer::get_video_width() const
{
  return SizeT( video_.get_info().width );
}

SizeT
AlfalfaVideoServer::get_video_height() const
{
  return SizeT( video_.get_info().height );
}

AlfalfaProtobufs::AlfalfaResponse
AlfalfaVideoServer::handle_request( const AlfalfaProtobufs::AlfalfaRequest & request ) const
{
  AlfalfaRequestType request_type = (AlfalfaRequestType) request.request_type();
  AlfalfaProtobufs::AlfalfaResponse response = get_alfalfa_response_protobuf();

  switch ( request_type ) {
    case AlfalfaRequestType::GetTrackSize:
      if ( not request.has_sizet() )
        throw runtime_error( "Request does not contain track_id" );
      *response.mutable_sizet() = get_track_size( SizeT( request.sizet() ) ).to_protobuf();
      break;
    case AlfalfaRequestType::GetFrameFrameInfo:
      if ( not request.has_sizet() )
        throw runtime_error( "Request does not contain frame_id" );
      *response.mutable_frame_info() =
        get_frame( SizeT( request.sizet() ) ).to_protobuf();
      break;
    case AlfalfaRequestType::GetFrameTrackData:
      if ( not request.has_track_position() )
        throw runtime_error( "Request does not contain track_position" );
      *response.mutable_track_data() =
        get_frame( TrackPosition( request.track_position() ) ).to_protobuf();
      break;
    case AlfalfaRequestType::GetRaster:
      if ( not request.has_sizet() )
        throw runtime_error( "Request does not contain raster_index" );
      *response.mutable_sizet() = get_raster( SizeT( request.sizet() ) ).to_protobuf();
      break;
    case AlfalfaRequestType::GetFramesTrack:
      if ( not request.has_track_range_args() )
        throw runtime_error( "Request does not contain track_range_args" );
      *response.mutable_frame_iterator() =
        get_frames( TrackRangeArgs( request.track_range_args() ) ).to_protobuf();
      break;
    case AlfalfaRequestType::GetFramesReverse:
      if ( not request.has_track_range_args() )
        throw runtime_error( "Request does not contain track_range_args" );
      *response.mutable_frame_iterator() =
        get_frames_reverse( TrackRangeArgs( request.track_range_args() ) ).to_protobuf();
      break;
    case AlfalfaRequestType::GetFramesSwitch:
      if ( not request.has_switch_range_args() )
        throw runtime_error( "Request does not contain switch_range_args" );
      *response.mutable_frame_iterator() =
        get_frames( SwitchRangeArgs( request.switch_range_args() ) ).to_protobuf();
      break;
    case AlfalfaRequestType::GetQualityDataByOriginalRaster:
      if ( not request.has_sizet() )
        throw runtime_error( "Request does not contain original raster" );
      *response.mutable_quality_data_iterator() =
        get_quality_data_by_original_raster( SizeT( request.sizet() ) ).to_protobuf();
      break;
    case AlfalfaRequestType::GetFramesByOutputHash:
      if ( not request.has_sizet() )
        throw runtime_error( "Request does not contain output hash" );
      *response.mutable_frame_iterator() =
        get_frames_by_output_hash( SizeT( request.sizet() ) ).to_protobuf();
      break;
    case AlfalfaRequestType::GetFramesByDecoderHash:
      if ( not request.has_decoder_hash() )
        throw runtime_error( "Request does not contain decoder hash" );
      *response.mutable_frame_iterator() =
        get_frames_by_decoder_hash( DecoderHash( request.decoder_hash() ) ).to_protobuf();
      break;
    case AlfalfaRequestType::GetTrackIds:
      *response.mutable_track_ids() = get_track_ids().to_protobuf();
      break;
    case AlfalfaRequestType::GetTrackDataByFrameId:
      if ( not request.has_sizet() )
        throw runtime_error( "Request does not contain frame id" );
      *response.mutable_track_data_iterator() =
        get_track_data_by_frame_id( SizeT( request.sizet() ) ).to_protobuf();
      break;
    case AlfalfaRequestType::GetSwitchesEndingWithFrame:
      if ( not request.has_sizet() )
        throw runtime_error( "Request does not contain frame id" );
      *response.mutable_switches() =
        get_switches_ending_with_frame( SizeT( request.sizet() ) ).to_protobuf();
      break;
    case AlfalfaRequestType::GetChunk:
      if ( not request.has_frame_info() )
        throw runtime_error( "Request does not contain frame_info" );
      *response.mutable_chunk() =
        get_chunk( FrameInfo( request.frame_info() ) ).to_protobuf();
      break;
    case AlfalfaRequestType::GetVideoWidth:
      *response.mutable_sizet() = get_video_width().to_protobuf();
      break;
    case AlfalfaRequestType::GetVideoHeight:
      *response.mutable_sizet() = get_video_height().to_protobuf();
      break;
  }

  return response;
}
