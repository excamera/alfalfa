#include <curl/curl.h>

#include "frame_fetcher.hh"

using namespace std;

/* error category for CURLcodes */
class curl_error_category : public error_category
{
public:
  const char * name( void ) const noexcept override { return "curl_error_category"; }
  string message( const int errornum ) const noexcept override
  {
    return curl_easy_strerror( static_cast<CURLcode>( errornum ) );
  }
};

/* call wrapper for CURL API */
inline void CurlCall( const std::string & s_attempt, const CURLcode result )
{
  if ( result == CURLE_OK ) {
    return;
  }

  throw tagged_error( curl_error_category(), s_attempt, static_cast<int>( result ) );
}

/* use static global to initialize library and clean up afterwards */
/* pray for no fiascos:
   https://isocpp.org/wiki/faq/ctors#static-init-order */

class GlobalCURL
{
public:
  GlobalCURL()
  {
    SystemCall( "curl_global_init", curl_global_init( CURL_GLOBAL_NOTHING ) );
  }

  ~GlobalCURL()
  {
    curl_global_cleanup();
  }
};

static GlobalCURL global_curl_library;

/* cleanup for the CURL handle */
void FrameFetcher::CurlWrapper::Deleter::operator() ( CURL * x ) const
{
  curl_easy_cleanup( x );
}

CURL * notnull( CURL * const x )
{
  return x ? x : throw runtime_error( "curl_easy_init failed" );
}

FrameFetcher::CurlWrapper::CurlWrapper()
  : c( notnull( curl_easy_init() ) )
{}

template <typename X, typename Y>
void FrameFetcher::CurlWrapper::setopt( const X option, const Y & parameter )
{
  CurlCall( "curl_easy_setopt", curl_easy_setopt( c.get(), option, parameter ) );
}

void FrameFetcher::CurlWrapper::perform()
{
  CurlCall( "curl_easy_perform", curl_easy_perform( c.get() ) );
}

FrameFetcher::FrameFetcher( const string & framestore_url )
  : framestore_url_( framestore_url )
{}

static ssize_t response_appender( const char * const buffer,
				  const size_t size,
				  const size_t nmemb,
				  string * const response_string )
{
  const size_t byte_size = size * nmemb;
  response_string->append( buffer, byte_size );
  return byte_size;
}

string FrameFetcher::get_chunk( const FrameInfo & frame_info )
{
  CurlWrapper curl;

  /* make request to video URL */
  curl.setopt( CURLOPT_URL, framestore_url_.c_str() );

  /* add range header */
  const string range_beginning = to_string( frame_info.offset() );
  const string range_end = to_string( frame_info.offset() + frame_info.length() - 1 );
  const string range_string = range_beginning + "-" + range_end;
  curl.setopt( CURLOPT_RANGE, range_string.c_str() );

  /* tell CURL where to put the response */
  string response_body;

  curl.setopt( CURLOPT_WRITEFUNCTION, response_appender );
  curl.setopt( CURLOPT_WRITEDATA, &response_body );

  /* make the query */
  curl.perform();

  return response_body;
}
