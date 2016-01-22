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
  vector<FrameInfo> requests_;
  vector<string> coded_frames_ { requests_.size() };
  unordered_map<size_t, size_t> request_offset_to_index_ {};
  unordered_map<string, string> headers_ {};
  bool body_started_ {};
  bool multipart_ {};

  size_t range_start_ {};
  size_t bytes_so_far_ {};
  size_t range_length_ {};

public:
  string range_of_requests() const
  {
    string ret;
    for ( const FrameInfo & x : requests_ ) {
      if ( not ret.empty() ) {
	ret.append( "," );
      }

      ret.append( to_string( x.offset() ) + "-" + to_string( x.offset() + x.length() - 1 ) );
    }
    return ret;
  }

  const unordered_map<string, string> & headers() const { return headers_; }
  const vector<string> & coded_frames() const { return coded_frames_; }
  
  HTTPResponse( vector<FrameInfo> && desired_frames )
    : requests_( move( desired_frames ) )
  {
    for ( unsigned int i = 0; i < requests_.size(); i++ ) {
      request_offset_to_index_.emplace( requests_[ i ].offset(), i );
    }
  }

  void verify_sizes() const
  {
    for ( unsigned int i = 0; i < requests_.size(); i++ ) {
      if ( requests_.at( i ).length() != coded_frames_.at( i ).length() ) {
	throw runtime_error( "FrameFetcher length mismatch, expected "
			     + to_string( requests_.at( i ).length() )
			     + " and received "
			     + to_string( coded_frames_.at( i ).length() ) );
      }
    }
  }

  void initialize_new_body()
  {
    /* Step 1: does response have a content-range header
       (and is therefore a single chunk?) */
    const auto content_range = headers_.find( "Content-Range" );
    if ( content_range != headers_.end() ) {
      /* parse the header */
      if ( content_range->second.substr( 0, 6 ) != "bytes " ) {
	throw runtime_error( "can't parse Content-Range: " + content_range->second );
      }

      const string bytes = content_range->second.substr( 6 );
      const size_t dash = bytes.find( "-" );
      if ( dash == string::npos ) {
	throw runtime_error( "can't parse Content-Range: " + content_range->second );
      }

      /* set up for single-range response */
      multipart_ = false;
      
      const size_t start_of_range = stoul( bytes.substr( 0, dash ) );
      const size_t end_of_range = stoul( bytes.substr( dash + 1 ) );

      initialize_new_range( start_of_range, end_of_range );
    } else {
      throw runtime_error( "need content-range header XXX" );
    }
  }    

  void initialize_new_range( const size_t start_of_range, const size_t end_of_range )
  {
    if ( bytes_so_far_ != range_length_ ) {
      throw runtime_error( "new range initialized before old range finished" );
    }

    range_start_ = start_of_range;
    range_length_ = end_of_range + 1 - start_of_range;
    bytes_so_far_ = 0;

    /* make sure there's a corresponding FrameInfo */
    if ( request_offset_to_index_.find( range_start_ ) == request_offset_to_index_.end() ) {
      throw runtime_error( "HTTP range received without corresponding request" );
    }
  }
  
  void new_body_chunk( const string & chunk )
  {
    if ( not body_started_ ) { /* first chunk of the body */
      body_started_ = true;
      initialize_new_body();
    }
      
    process_chunk( chunk );
  }

  void process_chunk( const string & chunk ) {
    if ( bytes_so_far_ + chunk.size() <= range_length_ ) {
      /* no worry of overlapping another request */
      coded_frames_.at( request_offset_to_index_.at( range_start_ ) ).append( chunk );
    } else {
      throw runtime_error( "can't deal with overlap" );
    }
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

vector<string> FrameFetcher::get_chunks( vector<FrameInfo> && frame_infos )
{
  HTTPResponse response { move( frame_infos ) };

  /* set range header */
  curl_.setopt( CURLOPT_RANGE, response.range_of_requests().c_str() );

  /* tell CURL where to put the headers and body */
  curl_.setopt( CURLOPT_HEADERDATA, &response );
  curl_.setopt( CURLOPT_WRITEDATA, &response );

  /* make the query */
  curl_.perform();

  response.verify_sizes();
  
  return response.coded_frames();
}

string FrameFetcher::get_chunk( const FrameInfo & frame_info )
{
  /* compatibility method */
  return get_chunks( { frame_info } ).front();
}
