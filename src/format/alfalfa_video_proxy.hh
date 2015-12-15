#ifndef ALFALFA_VIDEO_PROXY_HH
#define ALFALFA_VIDEO_PROXY_HH

#include "protobufs/alfalfa.pb.h"

#include "alfalfa_video.hh"

class AlfalfaVideoProxy 
{
private:
  const PlayableAlfalfaVideo video_;

public:
  AlfalfaVideoProxy( const std::string & directory_name )
    : video_( directory_name )
  {}

  /* Get size of a track. */
  size_t get_track_size( const size_t track_id ) const;

  /* Get a frame given a frame_id. */
  const FrameInfo & get_frame( const size_t frame_id ) const;

  /* Get a raster hash given raster_index. */
  size_t get_raster( const size_t raster_index ) const;

  /* Gets an iterator over quality data by approximate raster. */
  std::pair<QualityDBCollectionByOriginalRaster::iterator, QualityDBCollectionByOriginalRaster::iterator>
  get_quality_data_by_original_raster( const size_t original_raster) const;

  /* Gets an iterator over all frames by output hash / decoder hash. */
  std::pair<FrameDataSetCollectionByOutputHash::const_iterator, FrameDataSetCollectionByOutputHash::const_iterator>
  get_frames_by_output_hash( const size_t output_hash ) const;
  std::pair<FrameDataSetSourceHashSearch::const_iterator, FrameDataSetSourceHashSearch::const_iterator>
  get_frames_by_decoder_hash( const DecoderHash & decoder_hash ) const;

  /* Getters for width and height. */
  size_t get_video_width() const;
  size_t get_video_height() const;
};

#endif /* ALFALFA_VIDEO_PROXY_HH */
