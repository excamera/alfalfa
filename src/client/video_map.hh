#ifndef VIDEO_MAP_HH
#define VIDEO_MAP_HH

#include <vector>
#include <string>
#include <thread>

#include "alfalfa_video_client.hh"

class VideoMap
{
private:
  AlfalfaVideoClient video_;
  unsigned int max_track_id_;

  std::vector<size_t> track_lengths_ {};
  std::vector<std::vector<AlfalfaProtobufs::AbridgedFrameInfo>> tracks_ { max_track_id_ };
  std::vector<std::vector<size_t>> timestamp_to_frame_ {};

  std::thread fetcher_thread_;
  
  void fetch_all_tracks();
  bool tracks_still_to_fetch() const;
  
public:
  VideoMap( const std::string & server_address );
};

#endif /* VIDEO_MAP_HH */
