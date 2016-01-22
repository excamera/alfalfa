#ifndef FRAME_FETCHER_HH
#define FRAME_FETCHER_HH

#include <string>

#include "frame_info.hh"

class FrameFetcher
{
private:
  std::string framestore_url_;
  
public:
  FrameFetcher( const std::string & framestore_url );
  
  std::string get_chunk( const FrameInfo & frame_info );
};

#endif /* FRAME_FETCHER_HH */
