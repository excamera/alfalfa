#ifndef ALFALFA_VIDEO_CLIENT_HH
#define ALFALFA_VIDEO_CLIENT_HH

#include "protobufs/alfalfa.pb.h"

#include "alfalfa_video.hh"

class AlfalfaVideoClient
{
private:
  const PlayableAlfalfaVideo video_;

public:
  AlfalfaVideoClient( const std::string & directory_name )
    : video_( directory_name )
  {}

  /* Get size of a track. */
  size_t get_track_size( const size_t track_id ) const;

  /* Get a frame given a frame_id or given a track_id and frame_index. */
  const FrameInfo & get_frame( const size_t frame_id ) const;
  const TrackData & get_frame( const size_t track_id, const size_t frame_index ) const;

  /* Get a raster hash given raster_index. */
  size_t get_raster( const size_t raster_index ) const;

  /* Gets frames in the given track, between the provided indices. */
  std::pair<TrackDBIterator, TrackDBIterator>
  get_frames( const size_t track_id, const size_t start_frame_index, const size_t end_frame_index ) const;

  /* Gets frames in a track in reverse order, starting from the provided frame_index. */
  std::pair<TrackDBIterator, TrackDBIterator>
  get_frames_reverse( const size_t track_id, const size_t frame_index ) const;

  /* Gets switch frames between the provided indices. */
  std::pair<SwitchDBIterator, SwitchDBIterator>
  get_frames( const size_t from_track_id, const size_t to_track_id, const size_t from_frame_index,
              const size_t switch_start_index, const size_t switch_end_index ) const;

  /* Gets an iterator over quality data by approximate raster. */
  std::pair<QualityDBCollectionByOriginalRaster::iterator, QualityDBCollectionByOriginalRaster::iterator>
  get_quality_data_by_original_raster( const size_t original_raster) const;

  /* Gets an iterator over all frames by output hash / decoder hash. */
  std::pair<FrameDataSetCollectionByOutputHash::const_iterator, FrameDataSetCollectionByOutputHash::const_iterator>
  get_frames_by_output_hash( const size_t output_hash ) const;
  std::pair<FrameDataSetSourceHashSearch::const_iterator, FrameDataSetSourceHashSearch::const_iterator>
  get_frames_by_decoder_hash( const DecoderHash & decoder_hash ) const;

  /* Gets an iterator over all available track ids. */
  std::pair<std::unordered_set<size_t>::const_iterator, std::unordered_set<size_t>::const_iterator>
  get_track_ids() const;

  /* Gets an iterator over track data for a given frame_id. */
  std::pair<TrackDBCollectionByFrameIdIndex::const_iterator, TrackDBCollectionByFrameIdIndex::const_iterator>
  get_track_data_by_frame_id( const size_t frame_id ) const;

  std::vector<std::pair<SwitchDBIterator, SwitchDBIterator> >
  get_switches_ending_with_frame( const size_t frame_id ) const;

  /* Get chunk given a frame_info object. */
  const Chunk get_chunk( const FrameInfo & frame_info ) const;

  /* Getters for width and height. */
  size_t get_video_width() const;
  size_t get_video_height() const;
};

#endif /* ALFALFA_VIDEO_CLIENT_HH */
