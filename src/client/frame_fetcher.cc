#include <unordered_map>

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

class HTTPResponse
{
private:
  FrameInfo request_;
  unordered_map<string, string> headers_ {};
  string body_ {};

public:
  const unordered_map<string, string> & headers() const { return headers_; }
  const string & body() const { return body_; }
  
  HTTPResponse( const FrameInfo & desired_frame )
    : request_( desired_frame )
  {}

  void new_body_chunk( const string & chunk )
  {
    body_.append( chunk );
  }
  
  void new_header( const string & header_line )
  {
    /* parser taken from mahimahi http_header.cc */
    const string separator = ":";

    /* step 1: does buffer contain colon? */
    const size_t colon_location = header_line.find( separator );
    if ( colon_location == std::string::npos ) {
      return; /* status line or blank space, but not a header */
    }

    /* step 2: split buffer */
    const string key = header_line.substr( 0, colon_location );
    const string value_temp = header_line.substr( colon_location + separator.size() );

    /* step 3: strip whitespace */
    const string whitespace = " \r\n";
    const size_t first_nonspace = value_temp.find_first_not_of( whitespace );
    const string value_postfix = ( first_nonspace == string::npos ) /* handle case of all spaces */
      ? value_temp : value_temp.substr( first_nonspace );

    const size_t last_nonspace = value_temp.find_last_not_of( whitespace );
    const string value = ( last_nonspace == string::npos )
      ? value_postfix : value_postfix.substr( 0, last_nonspace );

    headers_.emplace( key, value );
  }
};

static size_t response_appender( const char * const buffer,
				 const size_t size,
				 const size_t nmemb,
				 HTTPResponse * const response )
{
  const size_t byte_size = size * nmemb;
  response->new_body_chunk( string( buffer, byte_size ) );
  return byte_size;
}

static size_t header_appender( const char * const buffer,
			       const size_t size,
			       const size_t nmemb,
			       HTTPResponse * const response )
{
  const size_t byte_size = size * nmemb;
  response->new_header( string( buffer, byte_size ) );
  return byte_size;
}

FrameFetcher::FrameFetcher( const string & framestore_url )
  : curl_()
{
  curl_.setopt( CURLOPT_URL, framestore_url.c_str() );
  curl_.setopt( CURLOPT_HEADERFUNCTION, header_appender );
  curl_.setopt( CURLOPT_WRITEFUNCTION, response_appender );
}

string FrameFetcher::get_chunk( const FrameInfo & frame_info )
{
  /* set range header */
  const string range_beginning = to_string( frame_info.offset() );
  const string range_end = to_string( frame_info.offset() + frame_info.length() - 1 );
  const string range_string = range_beginning + "-" + range_end;
  curl_.setopt( CURLOPT_RANGE, range_string.c_str() );

  HTTPResponse response { frame_info };

  /* tell CURL where to put the headers and body */
  curl_.setopt( CURLOPT_HEADERDATA, &response );
  curl_.setopt( CURLOPT_WRITEDATA, &response );

  /* make the query */
  curl_.perform();

  for ( const auto & x : response.headers() ) {
    cerr << x.first << ": {" << x.second << "}\n";
  }

  return response.body();
}
