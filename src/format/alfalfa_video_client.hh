#ifndef ALFALFA_VIDEO_CLIENT_HH
#define ALFALFA_VIDEO_CLIENT_HH

#include "alfalfa.pb.h"

#include "alfalfa_video_server.hh"

class AlfalfaVideoClient
{
private:
  const AlfalfaVideoServer server_;
  AlfalfaProtobufs::Chunk buffered_chunk_;

public:
  AlfalfaVideoClient( const std::string & directory_name )
    : server_( directory_name ),
      buffered_chunk_()
  {}

  /* Get size of a track. */
  size_t get_track_size( const size_t track_id ) const;

  /* Get a frame given a frame_id or given a track_id and frame_index. */
  FrameInfo get_frame( const size_t frame_id ) const;
  TrackData get_frame( const size_t track_id, const size_t frame_index ) const;

  /* Get a raster hash given raster_index. */
  size_t get_raster( const size_t raster_index ) const;

  /* Gets frames in the given track, between the provided indices. */
  std::vector<FrameInfo>
  get_frames( const size_t track_id, const size_t start_frame_index, const size_t end_frame_index ) const;

  /* Gets frames in a track in reverse order, starting from the provided frame_index. */
  std::vector<FrameInfo>
  get_frames_reverse( const size_t track_id, const size_t frame_index ) const;

  /* Gets switch frames between the provided indices. */
  std::vector<FrameInfo>
  get_frames( const size_t from_track_id, const size_t to_track_id, const size_t from_frame_index,
              const size_t switch_start_index, const size_t switch_end_index ) const;

  /* Gets an iterator over quality data by approximate raster. */
  std::vector<QualityData>
  get_quality_data_by_original_raster( const size_t original_raster) const;

  /* Gets an iterator over all frames by output hash / decoder hash. */
  std::vector<FrameInfo>
  get_frames_by_output_hash( const size_t output_hash ) const;
  std::vector<FrameInfo>
  get_frames_by_decoder_hash( const DecoderHash & decoder_hash ) const;

  /* Gets an iterator over all available track ids. */
  std::vector<size_t>
  get_track_ids() const;

  /* Gets an iterator over track data for a given frame_id. */
  std::vector<TrackData>
  get_track_data_by_frame_id( const size_t frame_id ) const;

  std::vector<SwitchInfo>
  get_switches_ending_with_frame( const size_t frame_id ) const;

  /* Get chunk given a frame_info object. */
  const Chunk get_chunk( const FrameInfo & frame_info );

  /* Getters for width and height. */
  size_t get_video_width() const;
  size_t get_video_height() const;
};

#endif /* ALFALFA_VIDEO_CLIENT_HH */
