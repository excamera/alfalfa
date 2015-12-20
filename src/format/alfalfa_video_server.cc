#include "alfalfa_video_server.hh"
#include "serialization.hh"

using namespace std;

AlfalfaProtobufs::SizeT
AlfalfaVideoServer::get_track_size( const AlfalfaProtobufs::SizeT & track_id ) const
{
  SizeT track_id_deserialized( track_id );
  return SizeT( video_.get_track_size( track_id_deserialized.sizet ) ).to_protobuf();
}

AlfalfaProtobufs::FrameInfo
AlfalfaVideoServer::get_frame( const AlfalfaProtobufs::SizeT & frame_id ) const
{
  SizeT frame_id_deserialized( frame_id );
  return video_.get_frame( frame_id_deserialized.sizet ).to_protobuf();
}

AlfalfaProtobufs::TrackData
AlfalfaVideoServer::get_frame( const AlfalfaProtobufs::TrackPosition & track_pos ) const
{
  TrackPosition track_pos_deserialized( track_pos );
  return video_.get_frame( track_pos_deserialized.track_id, track_pos_deserialized.frame_index ).to_protobuf();
}

AlfalfaProtobufs::SizeT
AlfalfaVideoServer::get_raster( const AlfalfaProtobufs::SizeT & raster_index ) const
{
  SizeT raster_index_deserialized( raster_index );
  return SizeT( video_.get_raster( raster_index_deserialized.sizet ).hash ).to_protobuf();
}

AlfalfaProtobufs::FrameIterator
AlfalfaVideoServer::get_frames( const AlfalfaProtobufs::TrackRangeArgs & args ) const
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
  return frame_iterator.to_protobuf();
}

AlfalfaProtobufs::FrameIterator
AlfalfaVideoServer::get_frames_reverse( const AlfalfaProtobufs::TrackPosition & track_pos ) const
{
  TrackPosition track_pos_deserialized( track_pos );
  auto frames = video_.get_frames_reverse( track_pos_deserialized.track_id,
                                           track_pos_deserialized.frame_index );
  FrameIterator frame_iterator;
  while ( frames.first != frames.second ) {
    frame_iterator.frames.push_back( *frames.first );
    frames.first--;
  }
  return frame_iterator.to_protobuf();
}

AlfalfaProtobufs::FrameIterator
AlfalfaVideoServer::get_frames( const AlfalfaProtobufs::SwitchRangeArgs & args ) const
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
  return frame_iterator.to_protobuf();
}

AlfalfaProtobufs::QualityDataIterator
AlfalfaVideoServer::get_quality_data_by_original_raster( const AlfalfaProtobufs::SizeT & original_raster ) const
{
  SizeT original_raster_deserialized( original_raster );
  auto quality_data = video_.get_quality_data_by_original_raster( original_raster_deserialized.sizet );
  QualityDataIterator quality_data_iterator;
  while ( quality_data.first != quality_data.second ) {
    quality_data_iterator.quality_data_items.push_back( *quality_data.first );
    quality_data.first++;
  }
  return quality_data_iterator.to_protobuf();
}

AlfalfaProtobufs::FrameIterator
AlfalfaVideoServer::get_frames_by_output_hash( const AlfalfaProtobufs::SizeT & output_hash ) const
{
  SizeT output_hash_deserialized( output_hash );
  auto frames = video_.get_frames_by_output_hash( output_hash_deserialized.sizet );
  FrameIterator frame_iterator;
  while ( frames.first != frames.second ) {
    frame_iterator.frames.push_back( *frames.first );
    frames.first++;
  }
  return frame_iterator.to_protobuf();
}

AlfalfaProtobufs::FrameIterator
AlfalfaVideoServer::get_frames_by_decoder_hash( const AlfalfaProtobufs::DecoderHash & decoder_hash ) const
{
  DecoderHash decoder_hash_deserialized( decoder_hash );
  auto frames = video_.get_frames_by_decoder_hash( decoder_hash );
  FrameIterator frame_iterator;
  while ( frames.first != frames.second ) {
    frame_iterator.frames.push_back( *frames.first );
    frames.first++;
  }
  return frame_iterator.to_protobuf();
}

AlfalfaProtobufs::TrackIdsIterator
AlfalfaVideoServer::get_track_ids() const
{
  auto track_ids = video_.get_track_ids();
  TrackIdsIterator track_ids_iterator;
  while ( track_ids.first != track_ids.second ) {
    track_ids_iterator.track_ids.push_back( *track_ids.first );
    track_ids.first++;
  }
  return track_ids_iterator.to_protobuf();
}

AlfalfaProtobufs::TrackDataIterator
AlfalfaVideoServer::get_track_data_by_frame_id( const AlfalfaProtobufs::SizeT & frame_id ) const
{
  SizeT frame_id_deserialized( frame_id );
  auto track_data = video_.get_track_data_by_frame_id( frame_id_deserialized.sizet );
  TrackDataIterator track_data_iterator;
  while ( track_data.first != track_data.second ) {
    track_data_iterator.track_data_items.push_back( *track_data.first );
    track_data.first++;
  }
  return track_data_iterator.to_protobuf();
}

AlfalfaProtobufs::Switches
AlfalfaVideoServer::get_switches_ending_with_frame( const AlfalfaProtobufs::SizeT & frame_id ) const
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
  return sw.to_protobuf();
}

const Chunk
AlfalfaVideoServer::get_chunk( const FrameInfo & frame_info ) const
{
  return video_.get_chunk( frame_info );
}

AlfalfaProtobufs::SizeT
AlfalfaVideoServer::get_video_width() const
{
  return SizeT( video_.get_info().width ).to_protobuf();
}

AlfalfaProtobufs::SizeT
AlfalfaVideoServer::get_video_height() const
{
  return SizeT( video_.get_info().height ).to_protobuf();
}
