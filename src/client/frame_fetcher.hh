#ifndef FRAME_FETCHER_HH
#define FRAME_FETCHER_HH

#include <memory>
#include <string>
#include <vector>

#include "frame_info.hh"
#include "frame_store.hh"

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
    CurlWrapper();
    template <typename X, typename Y> void setopt( const X option, const Y & parameter );

    void perform();
  };

  CurlWrapper curl_;
  FrameStore local_frame_store_;

public:
  FrameFetcher( const std::string & framestore_url );
  
  std::string get_chunk( const FrameInfo & frame_info );
  std::vector<std::string> get_chunks( const std::vector<FrameInfo> & frame_infos );
};

#endif /* FRAME_FETCHER_HH */
