#include <string>
#include <cstring>
#include <memory>

#include <netdb.h>

#include "address.hh"
#include "util.hh"

using namespace std;

/* constructors */

Address::Address()
  : size_( 0 ),
    addr_()
{}

Address::Address( const raw & addr, const size_t size )
  : Address( addr.as_sockaddr, size )
{}

Address::Address( const sockaddr & addr, const size_t size )
  : size_( size ),
    addr_()
{
  /* make sure proposed sockaddr can fit */
  if ( size > sizeof( addr_ ) ) {
    throw runtime_error( "invalid sockaddr size" );
  }

  memcpy( &addr_, &addr, size );
}

/* error category for getaddrinfo and getnameinfo */
class gai_error_category : public error_category
{
public:
  const char * name( void ) const noexcept override { return "gai_error_category"; }
  string message( const int return_value ) const noexcept override
  {
    return gai_strerror( return_value );
  }
};

/* private constructor given ip/host, service/port, and optional hints */
Address::Address( const string & node, const string & service, const addrinfo * hints )
  : size_(),
    addr_()
{
  /* prepare for the answer */
  addrinfo *resolved_address;

  /* look up the name or names */
  const int gai_ret = getaddrinfo( node.c_str(), service.c_str(), hints, &resolved_address );
  if ( gai_ret ) {
    throw tagged_error( gai_error_category(), "getaddrinfo", gai_ret );
  }

  /* if success, should always have at least one entry */
  if ( not resolved_address ) {
    throw runtime_error( "getaddrinfo returned successfully but with no results" );
  }
  
  /* put resolved_address in a wrapper so it will get freed if we have to throw an exception */
  struct Freeaddrinfo_Deleter { void operator()( addrinfo * const x ) const { freeaddrinfo( x ); } };
  unique_ptr<addrinfo, Freeaddrinfo_Deleter> wrapped_address( resolved_address );

  /* assign to our private members (making sure size fits) */
  *this = Address( *wrapped_address->ai_addr, wrapped_address->ai_addrlen );
}

/* construct by resolving host name and service name */
Address::Address( const std::string & hostname, const std::string & service )
  : Address()
{
  addrinfo hints;
  zero( hints );
  hints.ai_family = AF_INET6;
  hints.ai_flags = AI_V4MAPPED | AI_ALL;

  *this = Address( hostname, service, &hints );
}

/* construct with numerical IP address and numeral port number */
Address::Address( const std::string & ip, const uint16_t port )
  : Address()
{
  /* tell getaddrinfo that we don't want to resolve anything */
  addrinfo hints;
  zero( hints );
  hints.ai_family = AF_INET6;
  hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV | AI_V4MAPPED;

  *this = Address( ip, ::to_string( port ), &hints );
}

/* accessors */

pair<string, uint16_t> Address::ip_port( void ) const
{
  char ip[ NI_MAXHOST ], port[ NI_MAXSERV ];

  const int gni_ret = getnameinfo( &to_sockaddr(),
                                   size_,
                                   ip, sizeof( ip ),
                                   port, sizeof( port ),
                                   NI_NUMERICHOST | NI_NUMERICSERV );
  if ( gni_ret ) {
    throw tagged_error( gai_error_category(), "getnameinfo", gni_ret );
  }

  /* attempt to shorten v4-mapped address if possible */
  string ip_string { ip };
  if ( ip_string.size() > 7
       and ip_string.substr( 0, 7 ) == "::ffff:" ) {
    const string candidate_ipv4 = ip_string.substr( 7 );
    Address candidate_addr( candidate_ipv4, stoi( port ) );
    if ( candidate_addr == *this ) {
      ip_string = candidate_ipv4;
    }
  }

  return make_pair( ip_string, stoi( port ) );
}

string Address::to_string( void ) const
{
  const auto ip_and_port = ip_port();
  return ip_and_port.first + ":" + ::to_string( ip_and_port.second );
}

const sockaddr & Address::to_sockaddr( void ) const
{
  return addr_.as_sockaddr;
}

/* equality */
bool Address::operator==( const Address & other ) const
{
  return 0 == memcmp( &addr_, &other.addr_, size_ );
}
