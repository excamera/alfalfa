#include <unordered_map>
#include <chrono>

#include <curl/curl.h>

#include "frame_fetcher.hh"

using namespace std;
using namespace std::chrono;
using AbridgedFrameInfo = AlfalfaProtobufs::AbridgedFrameInfo;

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
inline void CurlCall( const string & s_attempt, const CURLcode result )
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
  vector<AbridgedFrameInfo> requests_;
  vector<string> coded_frames_ { requests_.size() };
  size_t current_frame_i_ = size_t( -1 );
  unordered_map<size_t, size_t> request_offset_to_index_ {};
  unordered_map<string, string> headers_ {};
  bool body_started_ {};
  bool multipart_ {};
  string multipart_boundary_ {};

  size_t range_start_ {};
  size_t bytes_so_far_ {};
  size_t range_length_ {};

  string residue_ {};

  FrameFetcher * parent_;
  
public:
  /* forbid copying */
  HTTPResponse( const HTTPResponse & other ) = delete;
  HTTPResponse & operator=( const HTTPResponse & other ) = delete;
  
  string range_of_requests() const
  {
    string ret;
    for ( const AbridgedFrameInfo & x : requests_ ) {
      if ( not ret.empty() ) {
	ret.append( "," );
      }

      ret.append( to_string( x.offset() ) + "-" + to_string( x.offset() + x.length() - 1 ) );
    }
    return ret;
  }

  const unordered_map<string, string> & headers() const { return headers_; }
  const vector<string> & coded_frames() const { return coded_frames_; }
  
  HTTPResponse( const vector<AbridgedFrameInfo> & desired_frames,
		FrameFetcher * parent )
    : requests_( desired_frames ),
      parent_( parent )
  {
    for ( unsigned int i = 0; i < requests_.size(); i++ ) {
      request_offset_to_index_.emplace( requests_[ i ].offset(), i );
    }
  }

  static pair<size_t, size_t> parse_content_range( const string & header_val )
  {
    if ( header_val.substr( 0, 6 ) != "bytes " ) {
      throw runtime_error( "can't parse Content-Range: " + header_val );
    }

    const string bytes = header_val.substr( 6 );
    const size_t dash = bytes.find( "-" );
    if ( dash == string::npos ) {
      throw runtime_error( "can't find dash in Content-Range: " + header_val );
    }

    return { stoul( bytes.substr( 0, dash ) ), stoul( bytes.substr( dash + 1 ) ) };
  }
  
  void initialize_new_body()
  {
    /* Step 1: does response have a content-range header
       (and is therefore a single chunk?) */
    const auto content_range = headers_.find( "Content-Range" );
    if ( content_range != headers_.end() ) {
      /* set up for single-range response */
      multipart_ = false;

      /* parse the header */
      const pair<size_t, size_t> start_and_end = parse_content_range( content_range->second );

      initialize_new_range( start_and_end.first, start_and_end.second );
    } else {
      /* multipart response? */
      const auto content_type = headers_.find( "Content-Type" );
      if ( content_type == headers_.end() ) {
	throw runtime_error( "HTTP response needs Content-Range or Content-Type" );
      }
      const string prefix = "multipart/byteranges; boundary=";
      if ( content_type->second.substr( 0, prefix.size() ) != prefix ) {
	throw runtime_error( "Expected multipart/byteranges; got " + content_type->second );
      }

      multipart_ = true;
      multipart_boundary_ = content_type->second.substr( prefix.size() );
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
    initialize_new_request();
  }

  void initialize_new_request()
  {
    if ( current_frame_i_ != size_t( -1 ) ) {
      insert();
    }

    const auto cur = request_offset_to_index_.find( range_start_ + bytes_so_far_ );
    if ( cur == request_offset_to_index_.end() ) {
      throw runtime_error( "HTTP response but could not find matching request" );
    }

    current_frame_i_ = cur->second;
  }
  
  void new_body_chunk( const string & chunk )
  {
    if ( not body_started_ ) { /* first chunk of the body */
      body_started_ = true;
      initialize_new_body();
    }

    if ( multipart_ ) {
      process_multipart_chunk( chunk );
    } else {
      process_simple_chunk( chunk );
    }
  }

  void process_simple_chunk( const string & chunk )
  {
    if ( bytes_so_far_ + chunk.size() > range_length_ ) {
      throw runtime_error( "single-part response is longer than advertised" );
    }

    size_t bytes_used_this_chunk = 0;
    while ( bytes_used_this_chunk < chunk.size() ) {
      /* is frame done? */
      if ( requests_.at( current_frame_i_ ).length()
	   == coded_frames_.at( current_frame_i_ ).length() ) {
	initialize_new_request();
      }

      const size_t bytes_left_in_frame = requests_.at( current_frame_i_ ).length()
	- coded_frames_.at( current_frame_i_ ).length();
      const size_t bytes_available = chunk.size() - bytes_used_this_chunk;
      const size_t bytes_to_append = min( bytes_left_in_frame, bytes_available );

      /* use the available bytes */
      coded_frames_.at( current_frame_i_ ).append( chunk.substr( bytes_used_this_chunk,
								 bytes_to_append ) );
      bytes_used_this_chunk += bytes_to_append;
      bytes_so_far_ += bytes_to_append;
      
      assert( coded_frames_.at( current_frame_i_ ).length() <= requests_.at( current_frame_i_ ).length() );
    }

    if ( bytes_so_far_ == range_length_ ) {
      insert();
    }
  }

  void insert()
  {
    if ( requests_.at( current_frame_i_ ).length()
	 != coded_frames_.at( current_frame_i_ ).length() ) {
      throw runtime_error( "BUG: frame length mismatch" );
    }
    parent_->insert_frame( requests_.at( current_frame_i_ ).offset(),
			   coded_frames_.at( current_frame_i_ ) );    
  }
  
  void process_multipart_boundary_header()
  {
    /* are there four newlines yet? */
    /* need initial CRLF, then "--" + boundary + CRLF, then header line + CRLF CRLF*/
    unsigned int newline_count = 0;
    size_t newline_index = 0;
    for ( const auto & ch : residue_ ) {
      if ( ch == '\n' ) {
	newline_count++;
      }

      if ( newline_count >= 4 ) {
	break;
      }

      newline_index++;
    }

    if ( newline_count < 4 ) {
      return;
    }

    const string boundary_phrase = "\r\n--" + multipart_boundary_ + "\r\n";
    if ( residue_.substr( 0, boundary_phrase.size() ) != boundary_phrase ) {
      throw runtime_error( "expected multipart boundary" );
    }

    const string header = residue_.substr( boundary_phrase.size(),
					   newline_index - boundary_phrase.size() - 1 );
    const Optional<size_t> colon_pos = colon_position( header );
    if ( not colon_pos.initialized() ) {
      throw runtime_error( "invalid multipart range header: " + header );
    }

    const pair<string, string> fields = parse_header( header, colon_pos.get() );
    if ( fields.first != "Content-range" ) {
      throw runtime_error( "invalid multipart range header key: " + header );
    }

    const pair<size_t, size_t> start_and_end = parse_content_range( fields.second );
    initialize_new_range( start_and_end.first, start_and_end.second );

    residue_.replace( 0, newline_index + 1, "" );
  }
  
  void process_multipart_chunk( const string & chunk )
  {
    residue_.append( chunk );

    while ( true ) {
      if ( residue_.empty() ) {
	return;
      }

      if ( range_length_ - bytes_so_far_ == 0 ) {
	/* current range is unknown */
	/* try to identify the new range */
	process_multipart_boundary_header();

	if ( range_length_ - bytes_so_far_ == 0 ) {
	  return; /* not enough bytes to identify a new range */
	}
      }

      /* we have a range and some available bytes to process */
      const size_t bytes_left_in_range = range_length_ - bytes_so_far_;
      const size_t bytes_available = residue_.size();
      const size_t bytes_to_process = min( bytes_left_in_range, bytes_available );
      process_simple_chunk( residue_.substr( 0, bytes_to_process ) );
      residue_.replace( 0, bytes_to_process, "" );
    }
  }

  static Optional<size_t> colon_position( const string & header_line )
  {
    /* step 1: does buffer contain colon? */
    const size_t colon_location = header_line.find( ":" );
    if ( colon_location == string::npos ) {
      return {}; /* status line or blank space, but not a header */
    } else {
      return { true, colon_location };
    }
  }
  
  static pair<string, string> parse_header( const string & header_line, const size_t colon_location )
  {
    /* parser taken from mahimahi http_header.cc */

    /* step 2: split buffer */
    const string key = header_line.substr( 0, colon_location );
    const string value_temp = header_line.substr( colon_location + 1 );

    /* step 3: strip whitespace */
    const string whitespace = " \r\n";
    const size_t first_nonspace = value_temp.find_first_not_of( whitespace );
    const string value_postfix = ( first_nonspace == string::npos ) /* handle case of all spaces */
      ? value_temp : value_temp.substr( first_nonspace );

    const size_t last_nonspace = value_temp.find_last_not_of( whitespace );
    const string value = ( last_nonspace == string::npos )
      ? value_postfix : value_postfix.substr( 0, last_nonspace );
    
    return { key, value };
  }

  void new_header( const string & header_line )
  {
    const Optional<size_t> colon_pos = colon_position( header_line );
    if ( colon_pos.initialized() ) {
      headers_.insert( parse_header( header_line, colon_pos.get() ) );
    }
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

FrameFetcher::CurlWrapper::CurlWrapper( const string & url )
  : c( notnull( curl_easy_init() ) )
{
  setopt( CURLOPT_URL, url.c_str() );
  setopt( CURLOPT_HEADERFUNCTION, header_appender );
  setopt( CURLOPT_WRITEFUNCTION, response_appender );
}

FrameFetcher::FrameFetcher( const string & framestore_url )
  : curl_( framestore_url ),
    estimated_bytes_per_second_( 10000 ),
    downloader_thread_( [&] () { event_loop(); } )
{}

FrameFetcher::~FrameFetcher()
{
  /* shut down the downloader thread */
  shutdown_ = true;
  new_request_or_shutdown_.notify_all();
  downloader_thread_.join();
}

void FrameFetcher::event_loop()
{
  const auto fetch_size = [] ( const vector<AbridgedFrameInfo> & frames )
    {
      unsigned int size = 0;
      for ( const auto & frame : frames ) {
	size += frame.length();
      }
      return size;
    };

  while ( not shutdown_ ) {
    /* fetch one batch of frames */
    vector<AbridgedFrameInfo> frames_to_fetch;
    unordered_set<uint64_t> frames_to_fetch_set;

    /* decide which frames we really need to fetch */
    {
      unique_lock<mutex> lock { mutex_ };

      for ( const auto & x : wishlist_ ) {
	if ( fetch_size( frames_to_fetch ) / estimated_bytes_per_second_ > 0.5 ) {
	  break;
	} else if ( frames_to_fetch.size() >= 96 ) {
	  break;
	}

	if ( local_frame_store_.has_frame( x.offset() ) ) {
	  /* skip it */
	} else if ( frames_to_fetch_set.count( x.offset() ) ) {
	  /* skip it */
	} else {
	  frames_to_fetch_set.emplace( x.offset() );
	  frames_to_fetch.push_back( x );
	}
      }
    }

    /* if nothing to fetch, wait and loop again */
    if ( frames_to_fetch.empty() ) {
      unique_lock<mutex> lock { mutex_ };
      new_request_or_shutdown_.wait( lock );
      continue;
    }

    assert( not frames_to_fetch.empty() );

    /* fetch the new round of frames */
    HTTPResponse response { frames_to_fetch, this };

    /* set range header */
    curl_.setopt( CURLOPT_RANGE, response.range_of_requests().c_str() );

    //    cerr << "fetching " << response.range_of_requests() << endl;
  
    /* tell CURL where to put the headers and body */
    curl_.setopt( CURLOPT_HEADERDATA, &response );
    curl_.setopt( CURLOPT_WRITEDATA, &response );

    /* make the query */

    const auto start_fetch_time = steady_clock::now();
    curl_.perform();
    const auto end_fetch_time = steady_clock::now();

    duration<double> time_in_seconds = end_fetch_time - start_fetch_time;

    //    cerr << "fetched " << fetch_size( frames_to_fetch ) << " bytes in " << time_in_seconds.count() << " seconds\n";
    
    const auto estimated_bytes_per_second_sample = fetch_size( frames_to_fetch ) / time_in_seconds.count();

    const double alpha = 1.0 / 4.0;
    estimated_bytes_per_second_ = (1 - alpha) * estimated_bytes_per_second_
      + alpha * estimated_bytes_per_second_sample;

    //    cerr << "estimated throughput now " << estimated_bytes_per_second_ * 8.0 / 1000.0 << " kilobits per second\n";
  }
}

void FrameFetcher::insert_frame( const uint64_t offset, const string & contents )
{
  {
    unique_lock<mutex> lock { mutex_ };
    local_frame_store_.insert_frame( offset, contents );
  }

  new_response_.notify_all();
}

bool FrameFetcher::has_frame( const AlfalfaProtobufs::AbridgedFrameInfo & frame )
{
  unique_lock<mutex> lock { mutex_ };
  return local_frame_store_.has_frame( frame.offset() );
}

void FrameFetcher::wait_for_frame( const AlfalfaProtobufs::AbridgedFrameInfo & frame )
{
  unique_lock<mutex> lock { mutex_ };
  new_response_.wait( lock, [&] () { return local_frame_store_.has_frame( frame.offset() ); } );    
}

bool FrameFetcher::is_plan_feasible()
{
  unique_lock<mutex> lock { mutex_ };
  return is_plan_feasible_nolock();
}

void FrameFetcher::wait_until_feasible()
{
  unique_lock<mutex> lock { mutex_ };
  new_response_.wait( lock, [&] () { return is_plan_feasible_nolock(); } );
}

string FrameFetcher::coded_frame( const AlfalfaProtobufs::AbridgedFrameInfo & frame )
{
  unique_lock<mutex> lock { mutex_ };
  if ( wishlist_.empty() ) {
    throw runtime_error( "coded_frame with empty plan" );
  } else if ( wishlist_.front().frame_id() != frame.frame_id() ) {
    throw runtime_error( "mismatch between requested frame and plan" );
  }

  wishlist_.pop_front();
  return local_frame_store_.coded_frame( frame.offset() );
}

string FrameFetcher::get_chunk( const FrameInfo & frame_info )
{
  /* compatibility method */
  AbridgedFrameInfo sub;

  sub.set_offset( frame_info.offset() );
  sub.set_length( frame_info.length() );
  sub.set_shown( frame_info.shown() );

  set_frame_plan( vector<AbridgedFrameInfo> { sub } );
  wait_for_frame( sub );
  return coded_frame( sub );
}

bool FrameFetcher::is_plan_feasible_nolock()
{
  const double discount_factor = 0.8;

  double presentation_time = 0.0;
  double arrival_time = 0.0;

  unsigned int frameno = 0;
  for ( const auto & frame : wishlist_ ) {
    if ( not local_frame_store_.has_frame( frame.offset() ) ) {
      /* need to download */
      arrival_time += frame.length() / (estimated_bytes_per_second_ * discount_factor);
    }

    if ( arrival_time > presentation_time ) {
      cerr << "failed feasibility @ frame " << frameno << ", " << arrival_time << " > " << presentation_time << "\n";
      return false;
    }

    if ( frame.shown() ) {
      presentation_time += 1.0 / 24.0;
    }

    frameno++;
  }
  
  return true;
}
