#ifndef ALFALFA_VIDEO_PROXY_HH
#define ALFALFA_VIDEO_PROXY_HH

#include "alfalfa.pb.h"

#include "alfalfa_video.hh"

class AlfalfaVideoServer 
{
private:
  const PlayableAlfalfaVideo video_;

  /* Get size of a track. */
  SizeT get_track_size( const SizeT & track_id ) const;

  /* Get a frame given a frame_id or given a track_id and frame_index. */
  FrameInfo get_frame( const SizeT & frame_id ) const;
  TrackData get_frame( const TrackPosition & track_pos ) const;

  /* Get a raster hash given raster_index. */
  SizeT get_raster( const SizeT & raster_index ) const;

  /* Gets frames in the given track, between the provided indices. */
  FrameIterator get_frames( const TrackRangeArgs & args ) const;

  /* Gets frames in a track in reverse order, starting from the provided frame index. */
  FrameIterator get_frames_reverse( const TrackRangeArgs & args ) const;

  /* Gets switch frames between the provided indices. */
  FrameIterator get_frames( const SwitchRangeArgs & args ) const;

  /* Gets an iterator over quality data by approximate raster. */
  QualityDataIterator get_quality_data_by_original_raster( const SizeT & original_raster ) const;

  /* Gets an iterator over all frames by output hash / decoder hash. */
  FrameIterator get_frames_by_output_hash( const SizeT & output_hash ) const;
  FrameIterator get_frames_by_decoder_hash( const DecoderHash & decoder_hash ) const;

  /* Gets an iterator over all available track ids. */
  TrackIdsIterator get_track_ids() const;

  /* Gets an iterator over track data for a given frame_id. */
  TrackDataIterator get_track_data_by_frame_id( const SizeT & frame_id ) const; 

  Switches get_switches_ending_with_frame( const SizeT & frame_id ) const; 

  Chunk get_chunk( const FrameInfo & frame_info ) const;

  /* Getters for width and height. */
  SizeT get_video_width() const;
  SizeT get_video_height() const;

public:
  AlfalfaVideoServer( const std::string & directory_name )
    : video_( directory_name )
  {}

  AlfalfaProtobufs::AlfalfaResponse handle_request( const AlfalfaProtobufs::AlfalfaRequest & request ) const;
};

#endif /* ALFALFA_VIDEO_PROXY_HH */
