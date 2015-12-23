#ifndef ALFALFA_VIDEO_PROXY_HH
#define ALFALFA_VIDEO_PROXY_HH

#include "alfalfa.pb.h"

#include "alfalfa_video.hh"

class AlfalfaVideoServer 
{
private:
  const PlayableAlfalfaVideo video_;

public:
  AlfalfaVideoServer( const std::string & directory_name )
    : video_( directory_name )
  {}

  /* Get size of a track. */
  AlfalfaProtobufs::SizeT get_track_size( const AlfalfaProtobufs::SizeT & track_id ) const;

  /* Get a frame given a frame_id or given a track_id and frame_index. */
  AlfalfaProtobufs::FrameInfo get_frame( const AlfalfaProtobufs::SizeT & frame_id ) const;
  AlfalfaProtobufs::TrackData get_frame( const AlfalfaProtobufs::TrackPosition & track_pos ) const;

  /* Get a raster hash given raster_index. */
  AlfalfaProtobufs::SizeT get_raster( const AlfalfaProtobufs::SizeT & raster_index ) const;

  /* Gets frames in the given track, between the provided indices. */
  AlfalfaProtobufs::FrameIterator get_frames( const AlfalfaProtobufs::TrackRangeArgs & args ) const;

  /* Gets frames in a track in reverse order, starting from the provided frame index. */
  AlfalfaProtobufs::FrameIterator get_frames_reverse( const AlfalfaProtobufs::TrackPosition & track_pos ) const;

  /* Gets switch frames between the provided indices. */
  AlfalfaProtobufs::FrameIterator get_frames( const AlfalfaProtobufs::SwitchRangeArgs & args ) const;

  /* Gets an iterator over quality data by approximate raster. */
  AlfalfaProtobufs::QualityDataIterator get_quality_data_by_original_raster( const AlfalfaProtobufs::SizeT & original_raster ) const;

  /* Gets an iterator over all frames by output hash / decoder hash. */
  AlfalfaProtobufs::FrameIterator get_frames_by_output_hash( const AlfalfaProtobufs::SizeT & output_hash ) const;
  AlfalfaProtobufs::FrameIterator get_frames_by_decoder_hash( const AlfalfaProtobufs::DecoderHash & decoder_hash ) const;

  /* Gets an iterator over all available track ids. */
  AlfalfaProtobufs::TrackIdsIterator get_track_ids() const;

  /* Gets an iterator over track data for a given frame_id. */
  AlfalfaProtobufs::TrackDataIterator get_track_data_by_frame_id( const AlfalfaProtobufs::SizeT & frame_id ) const;

  AlfalfaProtobufs::Switches get_switches_ending_with_frame( const AlfalfaProtobufs::SizeT & frame_id ) const;

  AlfalfaProtobufs::Chunk get_chunk( const AlfalfaProtobufs::FrameInfo & frame_info ) const;

  /* Getters for width and height. */
  AlfalfaProtobufs::SizeT get_video_width() const;
  AlfalfaProtobufs::SizeT get_video_height() const;
};

#endif /* ALFALFA_VIDEO_PROXY_HH */
