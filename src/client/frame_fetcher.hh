#ifndef FRAME_FETCHER_HH
#define FRAME_FETCHER_HH

#include <string>
#include <memory>

#include "frame_info.hh"

class HTTPClientWrapper;

class FrameFetcher
{
private:
  std::string framestore_url_;
  std::unique_ptr<HTTPClientWrapper> http_client_;
  
public:
  FrameFetcher( const std::string & framestore_url );
  ~FrameFetcher();
  
  std::string get_chunk( const FrameInfo & frame_info );
};

#endif /* FRAME_FETCHER_HH */
