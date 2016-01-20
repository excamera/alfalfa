#ifndef ALFALFA_VIDEO_SERVICE_HH
#define ALFALFA_VIDEO_SERVICE_HH

#include "alfalfa.pb.h"
#include "alfalfa.grpc.pb.h"

#include "alfalfa_video.hh"

class AlfalfaVideoServiceImpl : public AlfalfaProtobufs::AlfalfaVideo::Service
{
private:
  const PlayableAlfalfaVideo video_;
  std::string url_;
  
public:
  AlfalfaVideoServiceImpl( const std::string & directory_name,
			   const std::string & url )
    : video_( directory_name ),
      url_( url )
  {}

  const std::string & ivf_filename() const
  {
    return video_.ivf_filename();
  }
  
  /* NOTE: For better specifications, look at alfalfa_video_client.hh. */
  /* Get size of a track. */
  grpc::Status get_track_size( grpc::ServerContext * context,
                               const AlfalfaProtobufs::SizeT * track_id,
                               AlfalfaProtobufs::SizeT * track_size ) override;

  /* Get a frame given a frame_id or given a track_id and frame_index. */
  grpc::Status get_frame_by_id( grpc::ServerContext * context,
                                const AlfalfaProtobufs::SizeT * frame_id,
                                AlfalfaProtobufs::FrameInfo * frame_info ) override;

  grpc::Status get_frame_by_track_pos( grpc::ServerContext * context,
                                       const AlfalfaProtobufs::TrackPosition * track_pos,
                                       AlfalfaProtobufs::TrackData * track_data ) override;

  /* Get a raster hash given raster_index. */
  grpc::Status get_raster( grpc::ServerContext * context,
                           const AlfalfaProtobufs::SizeT * raster_index,
                           AlfalfaProtobufs::SizeT * raster_hash ) override;

  /* Gets frames in the given track, between the provided indices. */
  grpc::Status get_frames( grpc::ServerContext * context,
                           const AlfalfaProtobufs::TrackRangeArgs * args,
                           AlfalfaProtobufs::FrameIterator * frame_iterator ) override;

  /* Gets frames in a track in reverse order, starting from the provided frame index. */
  grpc::Status get_frames_reverse( grpc::ServerContext * context,
                                   const AlfalfaProtobufs::TrackRangeArgs * args,
                                   AlfalfaProtobufs::FrameIterator * frame_iterator ) override;

  /* Gets switch frames between the provided indices. */
  grpc::Status get_frames_in_switch( grpc::ServerContext * context,
                                     const AlfalfaProtobufs::SwitchRangeArgs * args,
                                     AlfalfaProtobufs::FrameIterator * frame_iterator ) override;

  /* Gets an iterator over quality data by approximate raster. */
  grpc::Status get_quality_data_by_original_raster( grpc::ServerContext * context,
                                                    const AlfalfaProtobufs::SizeT * original_raster,
                                                    AlfalfaProtobufs::QualityDataIterator * quality_iterator ) override;

  /* Gets an iterator over all frames by output hash / decoder hash. */
  grpc::Status get_frames_by_output_hash( grpc::ServerContext * context,
                                          const AlfalfaProtobufs::SizeT * output_hash,
                                          AlfalfaProtobufs::FrameIterator * frame_iterator ) override;

  grpc::Status get_frames_by_decoder_hash( grpc::ServerContext * context,
                                           const AlfalfaProtobufs::DecoderHash * decoder_hash,
                                           AlfalfaProtobufs::FrameIterator * frame_iterator ) override;

  /* Gets an iterator over all available track ids. */
  grpc::Status get_track_ids( grpc::ServerContext * context,
                              const AlfalfaProtobufs::Empty * empty,
                              AlfalfaProtobufs::TrackIdsIterator * track_ids_iterator ) override;

   /* Gets an iterator over track data for a given frame_id / track_id + displayed_raster_index. */
  grpc::Status get_track_data_by_frame_id( grpc::ServerContext * context,
                                           const AlfalfaProtobufs::SizeT * frame_id,
                                           AlfalfaProtobufs::TrackDataIterator * track_data_iterator ) override;

  grpc::Status get_frame_index_by_displayed_raster_index( grpc::ServerContext * context,
                                                          const AlfalfaProtobufs::TrackPositionDisplayedRasterIndex * track_pos_dri,
                                                          AlfalfaProtobufs::SizeT * response ) override;

  grpc::Status get_switches_ending_with_frame( grpc::ServerContext * context,
                                               const AlfalfaProtobufs::SizeT * frame_id,
                                               AlfalfaProtobufs::Switches * switches ) override;

  /* Getters for width and height. */
  grpc::Status get_video_width( grpc::ServerContext * context,
                                const AlfalfaProtobufs::Empty * empty,
                                AlfalfaProtobufs::SizeT * width ) override;

  grpc::Status get_video_height( grpc::ServerContext * context,
                                 const AlfalfaProtobufs::Empty * empty,
                                 AlfalfaProtobufs::SizeT * height ) override;

  /* Get URL to retrieve frames via HTTP */
  grpc::Status get_url( grpc::ServerContext * context,
			const AlfalfaProtobufs::Empty * empty,
			AlfalfaProtobufs::URL * url ) override;
};

#endif /* ALFALFA_VIDEO_SERVER_HH */
