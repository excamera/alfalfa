#ifndef FRAME_FETCHER_HH
#define FRAME_FETCHER_HH

#include <string>
#include <boost/network/protocol/http/client.hpp>

#include "frame_info.hh"

class FrameFetcher
{
private:
  std::string framestore_url_;
  boost::network::http::client http_client_;
  
public:
  FrameFetcher( const std::string & framestore_url );

  std::string get_chunk( const FrameInfo & frame_info );
};

#endif /* FRAME_FETCHER_HH */
