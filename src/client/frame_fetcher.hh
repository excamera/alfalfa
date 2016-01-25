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

  std::vector<AlfalfaProtobufs::AbridgedFrameInfo> wishlist_ {};

  std::atomic<bool> shutdown_ {};
  std::thread downloader_thread_;
  
  void event_loop();
  
public:
  FrameFetcher( const std::string & framestore_url );
  
  std::string get_chunk( const FrameInfo & frame_info );

  void set_frame_plan( const std::vector<AlfalfaProtobufs::AbridgedFrameInfo> & frames );
  std::string wait_for_frame( const AlfalfaProtobufs::AbridgedFrameInfo & frame );
  
  ~FrameFetcher();
};

#endif /* FRAME_FETCHER_HH */
