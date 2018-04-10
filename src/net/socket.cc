/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Copyright 2013-2018 the Alfalfa authors
                       and the Massachusetts Institute of Technology

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

      1. Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include <sys/socket.h>
#include "socket.hh"
#include "exception.hh"

using namespace std;

/* default constructor for socket of (subclassed) domain and type */
Socket::Socket( const int domain, const int type )
  : FileDescriptor( SystemCall( "socket", socket( domain, type, 0 ) ) )
{}

/* construct from file descriptor */
Socket::Socket( FileDescriptor && fd, const int domain, const int type )
  : FileDescriptor( move( fd ) )
{
  int actual_value;
  socklen_t len;

  /* verify domain */
  len = sizeof( actual_value );
  SystemCall( "getsockopt",
	      getsockopt( fd_num(), SOL_SOCKET, SO_DOMAIN, &actual_value, &len ) );
  if ( (len != sizeof( actual_value )) or (actual_value != domain) ) {
    throw runtime_error( "socket domain mismatch" );
  }

  /* verify type */
  len = sizeof( actual_value );
  SystemCall( "getsockopt",
	      getsockopt( fd_num(), SOL_SOCKET, SO_TYPE, &actual_value, &len ) );
  if ( (len != sizeof( actual_value )) or (actual_value != type) ) {
    throw runtime_error( "socket type mismatch" );
  }
}

/* get the local or peer address the socket is connected to */
Address Socket::get_address( const std::string & name_of_function,
			     const std::function<int(int, sockaddr *, socklen_t *)> & function ) const
{
  Address::raw address;
  socklen_t size = sizeof( address );

  SystemCall( name_of_function, function( fd_num(),
					  &address.as_sockaddr,
					  &size ) );

  return Address( address, size );
}

Address Socket::local_address( void ) const
{
  return get_address( "getsockname", getsockname );
}

Address Socket::peer_address( void ) const
{
  return get_address( "getpeername", getpeername );
}

/* bind socket to a specified local address (usually to listen/accept) */
void Socket::bind( const Address & address )
{
  SystemCall( "bind", ::bind( fd_num(),
			      &address.to_sockaddr(),
			      address.size() ) );
}

/* connect socket to a specified peer address */
void Socket::connect( const Address & address )
{
  SystemCall( "connect", ::connect( fd_num(),
				    &address.to_sockaddr(),
				    address.size() ) );
}

/* nanoseconds per millisecond */
static const uint64_t MILLION = 1000000;

/* nanoseconds per microsecond */
static const uint64_t THOUSAND = 1000;

/* nanoseconds per second */
static const uint64_t BILLION = 1000 * MILLION;

static uint64_t timestamp_us_raw( const timespec & ts )
{
  const uint64_t nanos = ts.tv_sec * BILLION + ts.tv_nsec;
  return nanos / THOUSAND;
}

/* receive datagram and where it came from */
UDPSocket::received_datagram UDPSocket::recv( void )
{
  static const ssize_t RECEIVE_MTU = 65536;

  /* receive source address, timestamp and payload */
  Address::raw datagram_source_address;
  msghdr header; zero( header );
  iovec msg_iovec; zero( msg_iovec );

  char msg_payload[ RECEIVE_MTU ];
  char msg_control[ RECEIVE_MTU ];

  /* prepare to get the source address */
  header.msg_name = &datagram_source_address;
  header.msg_namelen = sizeof( datagram_source_address );

  /* prepare to get the payload */
  msg_iovec.iov_base = msg_payload;
  msg_iovec.iov_len = sizeof( msg_payload );
  header.msg_iov = &msg_iovec;
  header.msg_iovlen = 1;

  /* prepare to get the timestamp */
  header.msg_control = msg_control;
  header.msg_controllen = sizeof( msg_control );

  /* call recvmsg */
  ssize_t recv_len = SystemCall( "recvmsg",
				 recvmsg( fd_num(), &header, 0 ) );

  /* make sure we got the whole datagram */
  if ( header.msg_flags & MSG_TRUNC ) {
    throw runtime_error( "recvfrom (oversized datagram)" );
  } else if ( header.msg_flags ) {
    throw runtime_error( "recvfrom (unhandled flag)" );
  }

  uint64_t timestamp_us = -1;

  /* find the timestamp header (if there is one) */
  cmsghdr *ts_hdr = CMSG_FIRSTHDR( &header );
  while ( ts_hdr ) {
    if ( ts_hdr->cmsg_level == SOL_SOCKET
	 and ts_hdr->cmsg_type == SO_TIMESTAMPNS ) {
      const timespec * const kernel_time = reinterpret_cast<timespec *>( CMSG_DATA( ts_hdr ) );
      timestamp_us = timestamp_us_raw( *kernel_time );
    }
    ts_hdr = CMSG_NXTHDR( &header, ts_hdr );
  }

  received_datagram ret = { Address( datagram_source_address,
                                     header.msg_namelen ),
                            timestamp_us,
                            string( msg_payload, recv_len ) };

  register_read();

  return ret;
}

/* send datagram to specified address */
void UDPSocket::sendto( const Address & destination, const string & payload )
{
  const ssize_t bytes_sent =
    SystemCall( "sendto", ::sendto( fd_num(),
				    payload.data(),
				    payload.size(),
				    0,
				    &destination.to_sockaddr(),
				    destination.size() ) );

  if ( size_t( bytes_sent ) != payload.size() ) {
    throw runtime_error( "datagram payload too big for sendto()" );
  }

  register_write();
}

/* send datagram to connected address */
void UDPSocket::send( const string & payload )
{
  const ssize_t bytes_sent =
    SystemCall( "send", ::send( fd_num(),
				payload.data(),
				payload.size(),
				0 ) );

  if ( size_t( bytes_sent ) != payload.size() ) {
    throw runtime_error( "datagram payload too big for send()" );
  }

  register_write();
}

/* set socket option */
template <typename option_type>
void Socket::setsockopt( const int level, const int option, const option_type & option_value )
{
  SystemCall( "setsockopt", ::setsockopt( fd_num(), level, option,
					  &option_value, sizeof( option_value ) ) );
}

/* allow local address to be reused sooner, at the cost of some robustness */
void Socket::set_reuseaddr( void )
{
  setsockopt( SOL_SOCKET, SO_REUSEADDR, int( true ) );
}

/* turn on timestamps on receipt */
void UDPSocket::set_timestamps( void )
{
  setsockopt( SOL_SOCKET, SO_TIMESTAMPNS, int( true ) );
}
