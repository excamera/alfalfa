#ifndef VIDEO_MAP_HH
#define VIDEO_MAP_HH

#include <vector>
#include <string>
#include <thread>
#include <mutex>

#include "alfalfa_video_client.hh"

class VideoMap
{
private:
  AlfalfaVideoClient video_;

  std::vector<size_t> track_lengths_;
  std::vector<std::vector<AlfalfaProtobufs::AbridgedFrameInfo>> tracks_ { track_lengths_.size() };
  std::vector<std::thread> fetchers_ {};
  std::vector<std::vector<size_t>> timestamp_to_frame_ {};

  void fetch_track( unsigned int track_id );

  mutable std::mutex mutex_ {};
  
public:
  VideoMap( const std::string & server_address );

  unsigned int track_length_full( const unsigned int track_id ) const;
  unsigned int track_length_now( const unsigned int track_id ) const;
  std::vector<AlfalfaProtobufs::AbridgedFrameInfo> track_snapshot( const unsigned int track_id,
								   const unsigned int start_frame_index ) const;
};

#endif /* VIDEO_MAP_HH */
