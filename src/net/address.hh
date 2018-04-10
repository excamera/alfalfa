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

#ifndef ADDRESS_HH
#define ADDRESS_HH

#include <string>
#include <utility>
#include <cstring>

#include <netinet/in.h>
#include <netdb.h>

/* Address class for IPv4/IPv6 addresses */
class Address
{
public:
  typedef union {
    sockaddr as_sockaddr;
    sockaddr_storage as_sockaddr_storage;
  } raw;

private:
  socklen_t size_;

  raw addr_;

  /* private constructor given ip/host, service/port, and optional hints */
  Address( const std::string & node, const std::string & service, const addrinfo * hints );

public:
  /* constructors */
  Address();
  Address( const raw & addr, const size_t size );
  Address( const sockaddr & addr, const size_t size );

  /* construct by resolving host name and service name */
  Address( const std::string & hostname, const std::string & service );

  /* construct with numerical IP address and numeral port number */
  Address( const std::string & ip, const uint16_t port );

  /* accessors */
  std::pair<std::string, uint16_t> ip_port( void ) const;
  std::string ip( void ) const { return ip_port().first; }
  uint16_t port( void ) const { return ip_port().second; }
  std::string to_string( void ) const;

  socklen_t size( void ) const { return size_; }
  const sockaddr & to_sockaddr( void ) const;

  /* equality */
  bool operator==( const Address & other ) const;
};

template <typename T> void zero( T & x ) { memset( &x, 0, sizeof( x ) ); }

#endif /* ADDRESS_HH */
