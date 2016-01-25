#ifndef FRAME_FETCHER_HH
#define FRAME_FETCHER_HH

#include <memory>
#include <string>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <atomic>

#include "frame_info.hh"
#include "frame_store.hh"
#include "alfalfa.pb.h"

typedef void CURL;

class FrameFetcher
{
private:
  class CurlWrapper
  {
  private:
    struct Deleter { void operator()( CURL * x ) const; };
    std::unique_ptr<CURL, Deleter> c;
  public:
    CurlWrapper( const std::string & url );
    template <typename X, typename Y> void setopt( const X option, const Y & parameter );

    void perform();
  };

  CurlWrapper curl_;
  FrameStore local_frame_store_ {};

  std::mutex mutex_ {};
  std::condition_variable new_request_or_shutdown_ {};
  std::condition_variable new_response_ {};

  std::deque<AlfalfaProtobufs::AbridgedFrameInfo> wishlist_ {};

  std::atomic<bool> shutdown_ {};
  std::atomic<double> estimated_bytes_per_second_;

  std::thread downloader_thread_;
  
  void event_loop();

  bool is_plan_feasible_nolock();
  
public:
  FrameFetcher( const std::string & framestore_url );
  
  std::string get_chunk( const FrameInfo & frame_info );

  template <typename Iterable>
  void set_frame_plan( const Iterable & frames )
  {
    std::unique_lock<std::mutex> lock { mutex_ };
    wishlist_.clear();
    wishlist_.insert( wishlist_.begin(), frames.begin(), frames.end() );
    new_request_or_shutdown_.notify_all();
  }

  bool has_frame( const AlfalfaProtobufs::AbridgedFrameInfo & frame );
  void wait_for_frame( const AlfalfaProtobufs::AbridgedFrameInfo & frame );
  std::string coded_frame( const AlfalfaProtobufs::AbridgedFrameInfo & frame );

  bool is_plan_feasible();
  void wait_until_feasible();
  
  ~FrameFetcher();
};

#endif /* FRAME_FETCHER_HH */
