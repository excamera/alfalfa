#ifndef VIDEO_MAP_HH
#define VIDEO_MAP_HH

#include <vector>
#include <string>
#include <thread>
#include <mutex>

#include "alfalfa_video_client.hh"

struct AnnotatedFrameInfo
{
  /* fields in AbridgedFrameInfo */
  uint64_t offset;
  unsigned int length;
  unsigned int frame_id;
  bool key;
  bool shown;
  float quality;

  /* annotations */
  double average_quality_to_end {};
  double stddev_quality_to_end {};
  unsigned int timestamp {}; /* displayed raster index */

  double remaining_time_in_video {};
  double download_time_required_from_here {};

  AnnotatedFrameInfo( const AlfalfaProtobufs::AbridgedFrameInfo & fi );
  AnnotatedFrameInfo( const FrameInfo & fi );
};

class VideoMap
{
private:
  AlfalfaVideoClient video_;

  std::vector<size_t> track_lengths_;
  std::vector<std::vector<AnnotatedFrameInfo>> tracks_ { track_lengths_.size() };
  std::vector<std::thread> fetchers_ {};
  std::vector<std::vector<size_t>> timestamp_to_frame_ {};

  void fetch_track( unsigned int track_id );

  mutable std::mutex mutex_ {};
  
public:
  VideoMap( const std::string & server_address );

  unsigned int track_length_full( const unsigned int track_id ) const;
  unsigned int track_length_now( const unsigned int track_id ) const;
  std::vector<AnnotatedFrameInfo> track_snapshot( const unsigned int track_id,
						  const unsigned int start_frame_index ) const;
  void update_annotations( const double estimated_bytes_per_second,
			   const std::unordered_set<uint64_t> & frame_store );
};

#endif /* VIDEO_MAP_HH */
