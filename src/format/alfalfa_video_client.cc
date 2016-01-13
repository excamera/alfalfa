#include "alfalfa_video_client.hh"
#include "serialization.hh"

using namespace std;

size_t
AlfalfaVideoClient::get_track_size( const size_t track_id ) const
{
  AlfalfaProtobufs::SizeT track_id_serialized = SizeT( track_id ).to_protobuf();
  return SizeT( server_.get_track_size( track_id_serialized ) ).sizet;
}

FrameInfo
AlfalfaVideoClient::get_frame( const size_t frame_id ) const
{
  AlfalfaProtobufs::SizeT frame_id_serialized = SizeT( frame_id ).to_protobuf();
  return FrameInfo( server_.get_frame( frame_id_serialized ) );
}

TrackData
AlfalfaVideoClient::get_frame( const size_t track_id, const size_t frame_index ) const
{
  AlfalfaProtobufs::TrackPosition track_pos_serialized = TrackPosition( track_id,
                                                                        frame_index
                                                                      ).to_protobuf();
  return TrackData( server_.get_frame( track_pos_serialized ) );
}

size_t
AlfalfaVideoClient::get_raster( const size_t raster_index ) const
{
  AlfalfaProtobufs::SizeT raster_index_serialized = SizeT( raster_index ).to_protobuf();
  return SizeT( server_.get_raster( raster_index_serialized ) ).sizet;
}

vector<FrameInfo>
AlfalfaVideoClient::get_frames( const size_t track_id, const size_t start_frame_index,
                                const size_t end_frame_index ) const
{
  AlfalfaProtobufs::TrackRangeArgs args_serialized = TrackRangeArgs( track_id,
                                                                     start_frame_index,
                                                                     end_frame_index
                                                                   ).to_protobuf();
  return FrameIterator( server_.get_frames( args_serialized ) ).frames;
}

vector<FrameInfo>
AlfalfaVideoClient::get_frames_reverse( const size_t track_id, const size_t start_frame_index, const size_t end_frame_index ) const
{
  AlfalfaProtobufs::TrackRangeArgs args_serialized =
    TrackRangeArgs( track_id,
                    start_frame_index,
                    end_frame_index
                  ).to_protobuf();
  return FrameIterator( server_.get_frames_reverse( args_serialized ) ).frames;
}

vector<FrameInfo>
AlfalfaVideoClient::get_frames( const size_t from_track_id, const size_t to_track_id,
                                const size_t from_frame_index, const size_t switch_start_index,
                                const size_t switch_end_index ) const
{
  AlfalfaProtobufs::SwitchRangeArgs args_serialized = SwitchRangeArgs( from_track_id,
                                                                       to_track_id,
                                                                       from_frame_index,
                                                                       switch_start_index,
                                                                       switch_end_index
                                                                     ).to_protobuf();

  return FrameIterator( server_.get_frames( args_serialized ) ).frames;
}

vector<QualityData>
AlfalfaVideoClient::get_quality_data_by_original_raster( const size_t original_raster ) const
{
  AlfalfaProtobufs::SizeT original_raster_serialized = SizeT( original_raster ).to_protobuf();
  return QualityDataIterator( server_.get_quality_data_by_original_raster( original_raster_serialized ) ).quality_data_items;
}

vector<FrameInfo>
AlfalfaVideoClient::get_frames_by_output_hash( const size_t output_hash ) const
{
  AlfalfaProtobufs::SizeT output_hash_serialized = SizeT( output_hash ).to_protobuf();
  return FrameIterator( server_.get_frames_by_output_hash( output_hash_serialized ) ).frames;
}

vector<FrameInfo>
AlfalfaVideoClient::get_frames_by_decoder_hash( const DecoderHash & decoder_hash ) const
{
  AlfalfaProtobufs::DecoderHash decoder_hash_serialized = decoder_hash.to_protobuf();
  return FrameIterator( server_.get_frames_by_decoder_hash( decoder_hash_serialized ) ).frames;
}

vector<size_t>
AlfalfaVideoClient::get_track_ids() const
{
  return TrackIdsIterator( server_.get_track_ids() ).track_ids;
}

vector<TrackData>
AlfalfaVideoClient::get_track_data_by_frame_id( const size_t frame_id ) const
{
  AlfalfaProtobufs::SizeT frame_id_serialized = SizeT( frame_id ).to_protobuf();
  return TrackDataIterator( server_.get_track_data_by_frame_id( frame_id_serialized ) ).track_data_items;
}

vector<TrackData>
AlfalfaVideoClient::get_track_data_by_displayed_raster_index( const size_t track_id,
                                                              const size_t displayed_raster_index ) const
{
  AlfalfaProtobufs::TrackPositionDisplayedRasterIndex track_pos_dri_serialized =
    TrackPositionDisplayedRasterIndex( track_id, displayed_raster_index ).to_protobuf();
  return TrackDataIterator( server_.get_track_data_by_displayed_raster_index( track_pos_dri_serialized ) ).track_data_items;
}

vector<SwitchInfo>
AlfalfaVideoClient::get_switches_ending_with_frame( const size_t frame_id ) const
{
  AlfalfaProtobufs::SizeT frame_id_serialized = SizeT( frame_id ).to_protobuf();
  return Switches( server_.get_switches_ending_with_frame( frame_id_serialized ) ).switches;
}

const Chunk
AlfalfaVideoClient::get_chunk( const FrameInfo & frame_info )
{
  AlfalfaProtobufs::FrameInfo frame_info_serialized = frame_info.to_protobuf();
  buffered_chunk_ = server_.get_chunk( frame_info_serialized );
  return Chunk( buffered_chunk_ );
}

size_t
AlfalfaVideoClient::get_video_width() const
{
  return SizeT( server_.get_video_width() ).sizet;
}

size_t
AlfalfaVideoClient::get_video_height() const
{
  return SizeT( server_.get_video_height() ).sizet;
}
