#ifndef VIDEO_MAP_HH
#define VIDEO_MAP_HH

#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <deque>
#include <atomic>

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
  double min_quality_to_end {};
  unsigned int timestamp; /* displayed raster index */

  double time_margin_required {};

  unsigned int track_id;
  unsigned int track_index;

  uint64_t cumulative_length;
  
  AnnotatedFrameInfo( const AlfalfaProtobufs::AbridgedFrameInfo & fi,
		      const unsigned int timestamp,
		      const unsigned int track_id,
		      const unsigned int track_index,
		      const uint64_t cumulative_length );
  AnnotatedFrameInfo( const FrameInfo & fi );
};

class VideoMap
{
private:
  AlfalfaVideoClient video_;

  std::vector<size_t> track_lengths_;
  std::vector<std::vector<AnnotatedFrameInfo>> tracks_;
  std::vector<unsigned int> shown_frame_counts_;
  std::vector<std::thread> fetchers_ {};
  std::unordered_multimap<uint64_t, std::pair<unsigned int, unsigned int>> keyframe_switches_ {};
  /* timestamp (dri) => track_id, track_index */

  unsigned int total_shown_frame_count_;
  
  void fetch_track( unsigned int track_id );

  mutable std::mutex mutex_ {};

  std::mutex annotation_mutex_ {};

  std::atomic_uint analysis_generation_ {};
  
public:
  VideoMap( const std::string & server_address, const unsigned int raster_count );

  unsigned int track_length_full( const unsigned int track_id ) const;
  unsigned int track_length_now( const unsigned int track_id ) const;
  std::deque<AnnotatedFrameInfo> best_plan( unsigned int track_id,
					    unsigned int track_index ) const;
  void update_annotations( const double estimated_bytes_per_second,
			   const std::unordered_map<uint64_t, std::pair<uint64_t, size_t>> frame_store );

  unsigned int analysis_generation() const { return analysis_generation_; }

  void report_feasibility() const;
};

#endif /* VIDEO_MAP_HH */
