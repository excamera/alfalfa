#include <boost/network/protocol/http/client.hpp>

#include "frame_fetcher.hh"

using namespace std;
using namespace boost::network::http;

typedef basic_client<tags::http_keepalive_8bit_udp_resolve, 1, 1> alfalfa_http_client;

struct HTTPClientWrapper
{
  alfalfa_http_client c {};
};

FrameFetcher::FrameFetcher( const string & framestore_url )
  : framestore_url_( framestore_url ),
    http_client_( new HTTPClientWrapper )
{}

string FrameFetcher::get_chunk( const FrameInfo & frame_info )
{
  /* make request to video URL */
  alfalfa_http_client::request the_request { framestore_url_ };

  /* add range header */
  const string range_beginning = to_string( frame_info.offset() );
  const string range_end = to_string( frame_info.offset() + frame_info.length() - 1 );
  const string range = "bytes=" + range_beginning + "-" + range_end;
  add_header( the_request, "Range", range );

  /* make the query */
  auto the_response = http_client_->c.get( the_request );

  return body( the_response );
}

FrameFetcher::~FrameFetcher() {}
