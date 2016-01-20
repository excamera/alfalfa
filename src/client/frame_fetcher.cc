#include <boost/network/protocol/http/client.hpp>

#include "frame_fetcher.hh"

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
  /* make request to video URL */
  client::request the_request { framestore_url_ };

  /* add range header */
  const string range_beginning = to_string( frame_info.offset() );
  const string range_end = to_string( frame_info.offset() + frame_info.length() - 1 );
  const string range = "bytes=" + range_beginning + "-" + range_end;
  add_header( the_request, "Range", range );

  cerr << "Fetching " << range << "\n";
  
  /* make the query */
  client::response the_response = http_client_->c.get( the_request );

  const string body_of_response = body( the_response );
  
  cerr << "returning response\n";

  return body_of_response;
}

FrameFetcher::~FrameFetcher() {}
