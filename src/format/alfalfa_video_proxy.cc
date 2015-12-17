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

const TrackData &
AlfalfaVideoProxy::get_frame( const size_t track_id, const size_t frame_index ) const
{
  return video_.get_frame( track_id, frame_index );
}

size_t
AlfalfaVideoProxy::get_raster( const size_t raster_index ) const
{
  return video_.get_raster( raster_index ).hash;
}

pair<TrackDBIterator, TrackDBIterator>
AlfalfaVideoProxy::get_frames( const size_t track_id, const size_t start_frame_index,
                               const size_t end_frame_index ) const
{
  return video_.get_track_range( track_id, start_frame_index, end_frame_index );
}

pair<TrackDBIterator, TrackDBIterator>
AlfalfaVideoProxy::get_frames_reverse( const size_t track_id, const size_t frame_index ) const
{
  return video_.get_frames_reverse( track_id, frame_index );
}

pair<SwitchDBIterator, SwitchDBIterator>
AlfalfaVideoProxy::get_frames( const size_t from_track_id, const size_t to_track_id,
                               const size_t from_frame_index, const size_t switch_start_index,
                               const size_t switch_end_index ) const
{
  return video_.get_switch_range( from_track_id, to_track_id, from_frame_index,
                                  switch_start_index, switch_end_index );
}

pair<QualityDBCollectionByOriginalRaster::iterator, QualityDBCollectionByOriginalRaster::iterator>
AlfalfaVideoProxy::get_quality_data_by_original_raster( const size_t original_raster ) const
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

pair<unordered_set<size_t>::const_iterator, unordered_set<size_t>::const_iterator>
AlfalfaVideoProxy::get_track_ids() const
{
  return video_.get_track_ids();
}

pair<TrackDBCollectionByFrameIdIndex::const_iterator, TrackDBCollectionByFrameIdIndex::const_iterator>
AlfalfaVideoProxy::get_track_data_by_frame_id( const size_t frame_id ) const
{
  return video_.get_track_data_by_frame_id( frame_id );
}

vector<pair<SwitchDBIterator, SwitchDBIterator>>
AlfalfaVideoProxy::get_switches_ending_with_frame( const size_t frame_id ) const
{
  return video_.get_switches_ending_with_frame( frame_id );
}

const Chunk
AlfalfaVideoProxy::get_chunk( const FrameInfo & frame_info ) const
{
  return video_.get_chunk( frame_info );
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
