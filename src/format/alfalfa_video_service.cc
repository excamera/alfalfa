#include "alfalfa_video_service.hh"
#include "serialization.hh"

using namespace std;
using namespace grpc;

#include <chrono>

class Log
{
private:
  chrono::time_point<chrono::system_clock> start_ { chrono::system_clock::now() };

public:
  Log( const string & rpc ) { cerr << rpc << "..."; }
  ~Log()
  {
    auto difference = chrono::system_clock::now() - start_;
    int micros = chrono::duration_cast<chrono::microseconds>( difference ).count();
    cerr << " done (" << micros << " us)\n";
  }
};

Status AlfalfaVideoServiceImpl::get_track_size( ServerContext *,
                                                const AlfalfaProtobufs::SizeT * track_id,
                                                AlfalfaProtobufs::SizeT * response )
{
  Log( "get_track_size" );

  SizeT track_id_deserialized( *track_id );
  response->CopyFrom( SizeT( video_.get_track_size( track_id_deserialized.sizet ) ).to_protobuf() );

  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_frame_by_id( ServerContext *,
                                                 const AlfalfaProtobufs::SizeT * frame_id,
                                                 AlfalfaProtobufs::FrameInfo * response )

{
  Log( "get_frame_by_id" );
  
  SizeT frame_id_deserialized( *frame_id );
  response->CopyFrom( video_.get_frame( frame_id_deserialized.sizet ).to_protobuf() );

  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_frame_by_track_pos( ServerContext *,
                                                        const AlfalfaProtobufs::TrackPosition * track_pos,
                                                        AlfalfaProtobufs::TrackData * response )
{
  Log( "get_frame_by_track_pos" );
  
  TrackPosition track_pos_deserialized( *track_pos );
  response->CopyFrom( video_.get_frame( track_pos_deserialized.track_id,
                                        track_pos_deserialized.frame_index ).to_protobuf() );
  cerr << "get_frame_by_track_pos\n";

  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_raster( ServerContext *,
                                            const AlfalfaProtobufs::SizeT * raster_index,
                                            AlfalfaProtobufs::SizeT * response )
{
  Log( "get_raster" );

  SizeT raster_index_deserialized( *raster_index );
  response->CopyFrom( SizeT( video_.get_raster( raster_index_deserialized.sizet ).hash ).to_protobuf() );
  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_abridged_frames( ServerContext *,
						     const AlfalfaProtobufs::TrackRangeArgs * args,
						     grpc::ServerWriter<AlfalfaProtobufs::AbridgedFrameInfo> * response )
{
  Log( "get_abridged_frames" );

  const auto frames = video_.get_track_range( args->track_id(),
					      args->start_frame_index(),
					      args->end_frame_index() );
  for ( auto frame = frames.first; frame != frames.second; frame++ ) {
    AlfalfaProtobufs::AbridgedFrameInfo abridged_frame_info;
    abridged_frame_info.set_offset( frame->offset() );
    abridged_frame_info.set_length( frame->length() );
    abridged_frame_info.set_frame_id( frame->frame_id() );
    abridged_frame_info.set_key( frame->is_keyframe() );
    abridged_frame_info.set_shown( frame->shown() );

    if ( frame->shown() ) {
      const auto q = video_.get_quality_data_by_approximate_raster( frame->name().target.output_hash );
      if ( q.first == q.second ) {
	throw runtime_error( "no quality data found for " + frame->name().target.output_hash );
      }
      abridged_frame_info.set_quality( q.first->quality );
    }

    response->Write( abridged_frame_info );
  }

  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_frames( ServerContext *,
                                            const AlfalfaProtobufs::TrackRangeArgs * args,
                                            AlfalfaProtobufs::FrameIterator * response )
{
  Log( "get_frames" );

  TrackRangeArgs args_deserialized( *args );
  auto frames = video_.get_track_range( args_deserialized.track_id,
                                        args_deserialized.start_frame_index,
                                        args_deserialized.end_frame_index );
  FrameIterator frame_iterator;
  while ( frames.first != frames.second ) {
    frame_iterator.frames.push_back( *frames.first );
    frames.first++;
  }

  response->CopyFrom( frame_iterator.to_protobuf() );
  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_frames_reverse( ServerContext *,
                                                    const AlfalfaProtobufs::TrackRangeArgs * args,
                                                    AlfalfaProtobufs::FrameIterator * response )
{
  Log( "get_frames_reverse" );

  TrackRangeArgs args_deserialized( *args );
  auto frames = video_.get_frames_reverse( args_deserialized.track_id,
                                           args_deserialized.start_frame_index,
                                           args_deserialized.end_frame_index );
  FrameIterator frame_iterator;
  while ( frames.first != frames.second ) {
    frame_iterator.frames.push_back( *frames.first );
    frames.first--;
  }

  response->CopyFrom( frame_iterator.to_protobuf() );
  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_frames_in_switch( ServerContext *,
                                                      const AlfalfaProtobufs::SwitchRangeArgs * args,
                                                      AlfalfaProtobufs::FrameIterator * response )
{
  Log( "get_frames_in_switch" );

  SwitchRangeArgs args_deserialized( *args );
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

  response->CopyFrom( frame_iterator.to_protobuf() );
  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_quality_data_by_original_raster( ServerContext *,
                                                                     const AlfalfaProtobufs::SizeT * original_raster,
                                                                     AlfalfaProtobufs::QualityDataIterator * response )
{
  Log( "get_quality_data_by_original_raster" );

  SizeT original_raster_deserialized( *original_raster );
  auto quality_data = video_.get_quality_data_by_original_raster( original_raster_deserialized.sizet );
  QualityDataIterator quality_data_iterator;
  while ( quality_data.first != quality_data.second ) {
    quality_data_iterator.quality_data_items.push_back( *quality_data.first );
    quality_data.first++;
  }

  response->CopyFrom( quality_data_iterator.to_protobuf() );
  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_frames_by_output_hash( ServerContext *,
                                                           const AlfalfaProtobufs::SizeT * output_hash,
                                                           AlfalfaProtobufs::FrameIterator * response )
{
  Log( "get_frames_by_output_hash" );

  SizeT output_hash_deserialized( *output_hash );
  auto frames = video_.get_frames_by_output_hash( output_hash_deserialized.sizet );
  FrameIterator frame_iterator;
  while ( frames.first != frames.second ) {
    frame_iterator.frames.push_back( *frames.first );
    frames.first++;
  }

  response->CopyFrom( frame_iterator.to_protobuf() );
  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_frames_by_decoder_hash( ServerContext *,
                                                            const AlfalfaProtobufs::DecoderHash * decoder_hash,
                                                            AlfalfaProtobufs::FrameIterator * response )
{
  Log( "get_frames_by_decoder_hash" );

  DecoderHash decoder_hash_deserialized( *decoder_hash );
  auto frames = video_.get_frames_by_decoder_hash( decoder_hash_deserialized );
  FrameIterator frame_iterator;
  while ( frames.first != frames.second ) {
    frame_iterator.frames.push_back( *frames.first );
    frames.first++;
  }
  response->CopyFrom( frame_iterator.to_protobuf() );
  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_track_ids( ServerContext *,
                                               const AlfalfaProtobufs::Empty *,
                                               AlfalfaProtobufs::TrackIdsIterator * response )
{
  Log( "get_track_ids" );

  auto track_ids = video_.get_track_ids();
  TrackIdsIterator track_ids_iterator;
  while ( track_ids.first != track_ids.second ) {
    track_ids_iterator.track_ids.push_back( *track_ids.first );
    track_ids.first++;
  }

  response->CopyFrom( track_ids_iterator.to_protobuf() );
  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_track_data_by_frame_id( ServerContext *,
                                                            const AlfalfaProtobufs::SizeT * frame_id,
                                                            AlfalfaProtobufs::TrackDataIterator * response )
{
  Log( "get_track_data_by_frame_id" );
  
  SizeT frame_id_deserialized( *frame_id );
  auto track_data = video_.get_track_data_by_frame_id( frame_id_deserialized.sizet );
  TrackDataIterator track_data_iterator;
  while ( track_data.first != track_data.second ) {
    track_data_iterator.track_data_items.push_back( *track_data.first );
    track_data.first++;
  }

  response->CopyFrom( track_data_iterator.to_protobuf() );
  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_frame_index_by_displayed_raster_index( ServerContext *,
  const AlfalfaProtobufs::TrackPositionDisplayedRasterIndex * track_pos_dri,
  AlfalfaProtobufs::SizeT * response )
{
  Log( "get_frame_index_by_displayed_raster_index" );

  TrackPositionDisplayedRasterIndex track_pos_dri_deserialized( *track_pos_dri );
  size_t frame_index = video_.get_dri_to_frame_index_mapping( track_pos_dri_deserialized.track_id,
                                                              track_pos_dri_deserialized.displayed_raster_index ).front();
  response->CopyFrom( SizeT( frame_index ).to_protobuf() );
  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_switches_ending_with_frame( ServerContext *,
                                                                const AlfalfaProtobufs::SizeT * frame_id,
                                                                AlfalfaProtobufs::Switches * response )
{
  Log( "get_switches_ending_with_frame" );

  SizeT frame_id_deserialized( *frame_id );
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

  response->CopyFrom( sw.to_protobuf() );
  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_video_width( ServerContext *,
                                                 const AlfalfaProtobufs::Empty *,
                                                 AlfalfaProtobufs::SizeT * response )
{
  Log( "get_video_width" );

  response->CopyFrom( SizeT( video_.get_info().width ).to_protobuf() );
  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_video_height( ServerContext *,
                                                  const AlfalfaProtobufs::Empty *,
                                                  AlfalfaProtobufs::SizeT * response )
{
  Log( "get_video_height" );

  response->CopyFrom( SizeT( video_.get_info().height ).to_protobuf() );
  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_url( ServerContext *,
					 const AlfalfaProtobufs::Empty *,
					 AlfalfaProtobufs::URL * response )
{
  Log( "get_url" );

  response->set_url( url_ );
  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_raster_count( ServerContext *,
						  const AlfalfaProtobufs::Empty *,
						  AlfalfaProtobufs::SizeT * count )
{
  Log( "get_raster_count" );
  count->set_sizet( video_.get_raster_list_size() );
  return Status::OK;
}

Status AlfalfaVideoServiceImpl::get_outbound_switch( grpc::ServerContext *,
						     const AlfalfaProtobufs::OutboundSwitchQuery * query,
						     AlfalfaProtobufs::AbridgedSwitch * response )
{
  Log( "get_outbound_switch" );
  const auto switchdesc = video_.get_frames_switch( query->from_track_id(),
						    query->from_frame_index(),
						    query->to_track_id() );
  for ( auto it = switchdesc.first; it != switchdesc.second; it++ ) {
    response->set_from_track_id( it.from_track_id() );
    response->set_to_track_id( it.to_track_id() );
    response->set_from_frame_index( it.from_frame_index() );
    response->set_to_frame_index( it.to_frame_index() );

    AlfalfaProtobufs::AbridgedFrameInfo * fi = response->add_frame();
    fi->set_offset( it->offset() );
    fi->set_length( it->length() );
    fi->set_frame_id( it->frame_id() );
    fi->set_key( it->is_keyframe() );
    fi->set_shown( it->shown() );

    if ( it->shown() ) {
      const auto q = video_.get_quality_data_by_approximate_raster( it->name().target.output_hash );
      if ( q.first == q.second ) {
	throw runtime_error( "no quality data found for " + it->name().target.output_hash );
      }
      fi->set_quality( q.first->quality );
    }
  }

  return Status::OK;
}
