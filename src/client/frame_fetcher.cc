#include "frame_fetcher.hh"

using namespace std;

FrameFetcher::FrameFetcher( const string & framestore_url )
  : framestore_url_( framestore_url ),
    http_client_()
{}

string FrameFetcher::get_chunk( const FrameInfo & )
{
  return "";
}
