#include "frame_fetcher.hh"

#include <boost/network/protocol/http/client.hpp>

using namespace std;
using namespace boost::network::http;

struct HTTPClientWrapper
{
  client c {};
};

FrameFetcher::FrameFetcher( const string & framestore_url )
  : framestore_url_( framestore_url ),
    http_client_( new HTTPClientWrapper )
{}

string FrameFetcher::get_chunk( const FrameInfo & frame_info )
{
  client::request the_request;
  the_request << boost::network::destination( framestore_url_ );
  const string range_beginning = to_string( frame_info.offset() );
  const string range_end = to_string( frame_info.offset() + frame_info.length() - 1 );
  the_request << boost::network::header( "Range", "bytes=" + range_beginning + "-" + range_end );
  client::response the_response = http_client_->c.get( the_request );
  return body( the_response );
}

FrameFetcher::~FrameFetcher() {}
