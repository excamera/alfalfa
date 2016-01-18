#include "alfalfa_video_client.hh"
#include "serialization.hh"
#include "exception.hh"

using namespace std;
using namespace grpc;

size_t
AlfalfaVideoClient::get_track_size( const size_t track_id ) const
{
  ClientContext context;
  AlfalfaProtobufs::SizeT track_id_serialized = SizeT( track_id ).to_protobuf();
  AlfalfaProtobufs::SizeT response;
  RPC( "get_track_size",
    stub_->get_track_size( &context, track_id_serialized, &response ) );
  return SizeT( response ).sizet;
}

FrameInfo
AlfalfaVideoClient::get_frame( const size_t frame_id ) const
{
  ClientContext context;
  AlfalfaProtobufs::SizeT frame_id_serialized = SizeT( frame_id ).to_protobuf();
  AlfalfaProtobufs::FrameInfo response;
  RPC( "get_frame_by_id",
    stub_->get_frame_by_id( &context, frame_id_serialized, &response ) );
  return FrameInfo( response );
}

TrackData
AlfalfaVideoClient::get_frame( const size_t track_id, const size_t frame_index ) const
{
  ClientContext context;
  AlfalfaProtobufs::TrackPosition track_pos_serialized = TrackPosition( track_id,
                                                                        frame_index
                                                                      ).to_protobuf();
  AlfalfaProtobufs::TrackData response;
  RPC( "get_frame_by_track_pos",
    stub_->get_frame_by_track_pos( &context, track_pos_serialized, &response ) );
  return TrackData( response );
}

size_t
AlfalfaVideoClient::get_raster( const size_t raster_index ) const
{
  ClientContext context;
  AlfalfaProtobufs::SizeT raster_index_serialized = SizeT( raster_index ).to_protobuf();
  AlfalfaProtobufs::SizeT response;
  RPC( "get_raster",
    stub_->get_raster( &context, raster_index_serialized, &response ) );
  return SizeT( response ).sizet;
}

vector<FrameInfo>
AlfalfaVideoClient::get_frames( const size_t track_id, const size_t start_frame_index,
                                const size_t end_frame_index ) const
{
  ClientContext context;
  AlfalfaProtobufs::TrackRangeArgs args_serialized = TrackRangeArgs( track_id,
                                                                     start_frame_index,
                                                                     end_frame_index
                                                                   ).to_protobuf();
  AlfalfaProtobufs::FrameIterator response;
  RPC( "get_frames",
    stub_->get_frames( &context, args_serialized, &response ) );
  return FrameIterator( response ).frames;
}

vector<FrameInfo>
AlfalfaVideoClient::get_frames_reverse( const size_t track_id, const size_t start_frame_index, const size_t end_frame_index ) const
{
  ClientContext context;
  AlfalfaProtobufs::TrackRangeArgs args_serialized =
    TrackRangeArgs( track_id,
                    start_frame_index,
                    end_frame_index
                  ).to_protobuf();
  AlfalfaProtobufs::FrameIterator response;
  RPC( "get_frames_reverse",
    stub_->get_frames_reverse( &context, args_serialized, &response ) );
  return FrameIterator( response ).frames;
}

vector<FrameInfo>
AlfalfaVideoClient::get_frames( const size_t from_track_id, const size_t to_track_id,
                                const size_t from_frame_index, const size_t switch_start_index,
                                const size_t switch_end_index ) const
{
  ClientContext context;
  AlfalfaProtobufs::SwitchRangeArgs args_serialized = SwitchRangeArgs( from_track_id,
                                                                       to_track_id,
                                                                       from_frame_index,
                                                                       switch_start_index,
                                                                       switch_end_index
                                                                     ).to_protobuf();
  AlfalfaProtobufs::FrameIterator response;
  RPC( "get_frames_in_switch",
    stub_->get_frames_in_switch( &context, args_serialized, &response ) );
  return FrameIterator( response ).frames;
}

vector<QualityData>
AlfalfaVideoClient::get_quality_data_by_original_raster( const size_t original_raster ) const
{
  ClientContext context;
  AlfalfaProtobufs::SizeT original_raster_serialized = SizeT( original_raster ).to_protobuf();
  AlfalfaProtobufs::QualityDataIterator response;
  RPC( "get_quality_data_by_original_raster",
    stub_->get_quality_data_by_original_raster( &context, original_raster_serialized, &response ) );
  return QualityDataIterator( response ).quality_data_items;
}

vector<FrameInfo>
AlfalfaVideoClient::get_frames_by_output_hash( const size_t output_hash ) const
{
  ClientContext context;
  AlfalfaProtobufs::SizeT output_hash_serialized = SizeT( output_hash ).to_protobuf();
  AlfalfaProtobufs::FrameIterator response;
  RPC( "get_frames_by_output_hash",
    stub_->get_frames_by_output_hash( &context, output_hash_serialized, &response ) );
  return FrameIterator( response ).frames;
}

vector<FrameInfo>
AlfalfaVideoClient::get_frames_by_decoder_hash( const DecoderHash & decoder_hash ) const
{
  ClientContext context;
  AlfalfaProtobufs::DecoderHash decoder_hash_serialized = decoder_hash.to_protobuf();
  AlfalfaProtobufs::FrameIterator response;
  RPC( "get_frames_by_decoder_hash",
    stub_->get_frames_by_decoder_hash( &context, decoder_hash_serialized, &response ) );
  return FrameIterator( response ).frames;
}

vector<size_t>
AlfalfaVideoClient::get_track_ids() const
{
  ClientContext context;
  AlfalfaProtobufs::Empty empty;
  AlfalfaProtobufs::TrackIdsIterator response;
  RPC( "get_track_ids",
    stub_->get_track_ids( &context, empty, &response ) );
  return TrackIdsIterator( response ).track_ids;
}

vector<TrackData>
AlfalfaVideoClient::get_track_data_by_frame_id( const size_t frame_id ) const
{
  ClientContext context;
  AlfalfaProtobufs::SizeT frame_id_serialized = SizeT( frame_id ).to_protobuf();
  AlfalfaProtobufs::TrackDataIterator response;
  RPC( "get_track_data_by_frame_id",
    stub_->get_track_data_by_frame_id( &context, frame_id_serialized, &response ) );
  return TrackDataIterator( response ).track_data_items;
}

size_t
AlfalfaVideoClient::get_frame_index_by_displayed_raster_index( const size_t track_id,
                                                               const size_t displayed_raster_index ) const
{
  ClientContext context;
  AlfalfaProtobufs::TrackPositionDisplayedRasterIndex track_pos_dri_serialized =
    TrackPositionDisplayedRasterIndex( track_id, displayed_raster_index ).to_protobuf();
  AlfalfaProtobufs::SizeT response;
  RPC( "get_frame_index_by_displayed_raster_index",
    stub_->get_frame_index_by_displayed_raster_index( &context, track_pos_dri_serialized, &response )  );
  return SizeT( response ).sizet;
}

vector<SwitchInfo>
AlfalfaVideoClient::get_switches_ending_with_frame( const size_t frame_id ) const
{
  ClientContext context;
  AlfalfaProtobufs::SizeT frame_id_serialized = SizeT( frame_id ).to_protobuf();
  AlfalfaProtobufs::Switches response;
  RPC( "get_switches_ending_with_frame",
    stub_->get_switches_ending_with_frame( &context, frame_id_serialized, &response ) );
  return Switches( response ).switches;
}

string AlfalfaVideoClient::get_chunk( const FrameInfo & frame_info ) const
{
  ClientContext context;
  AlfalfaProtobufs::Chunk response;
  RPC( "get_chunk",
       stub_->get_chunk( &context, frame_info.to_protobuf(), &response ) );
  return response.buffer();
}

size_t
AlfalfaVideoClient::get_video_width() const
{
  ClientContext context;
  AlfalfaProtobufs::Empty empty;
  AlfalfaProtobufs::SizeT response;
  RPC( "get_video_width",
    stub_->get_video_width( &context, empty, &response ) );
  return SizeT( response ).sizet;
}

size_t
AlfalfaVideoClient::get_video_height() const
{
  ClientContext context;
  AlfalfaProtobufs::Empty empty;
  AlfalfaProtobufs::SizeT response;
  RPC( "get_video_height",
    stub_->get_video_height( &context, empty, &response ) );
  return SizeT( response ).sizet;
}
