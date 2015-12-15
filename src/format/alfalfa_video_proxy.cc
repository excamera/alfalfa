#include "alfalfa_video_proxy.hh"

using namespace std;

size_t
AlfalfaVideoProxy::get_track_size( const size_t track_id ) const
{
  return video_.get_track_size( track_id );
}

const FrameInfo &
AlfalfaVideoProxy::get_frame( const size_t frame_id ) const
{
  return video_.get_frame( frame_id );
}

size_t
AlfalfaVideoProxy::get_raster( const size_t raster_index ) const
{
  return video_.get_raster( raster_index ).hash;
}

pair<QualityDBCollectionByOriginalRaster::iterator, QualityDBCollectionByOriginalRaster::iterator>
AlfalfaVideoProxy::get_quality_data_by_original_raster( const size_t original_raster) const
{
  return video_.get_quality_data_by_original_raster( original_raster );
}

pair<FrameDataSetCollectionByOutputHash::const_iterator, FrameDataSetCollectionByOutputHash::const_iterator>
AlfalfaVideoProxy::get_frames_by_output_hash( const size_t output_hash ) const
{
  return video_.get_frames_by_output_hash( output_hash );
}

pair<FrameDataSetSourceHashSearch::const_iterator, FrameDataSetSourceHashSearch::const_iterator>
AlfalfaVideoProxy::get_frames_by_decoder_hash( const DecoderHash & decoder_hash ) const
{
  return video_.get_frames_by_decoder_hash( decoder_hash );
}

size_t
AlfalfaVideoProxy::get_video_width() const
{
  return video_.get_info().width;
}

size_t
AlfalfaVideoProxy::get_video_height() const
{
  return video_.get_info().height;
}
