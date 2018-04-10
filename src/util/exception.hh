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

#ifndef EXCEPTION_HH
#define EXCEPTION_HH

#include <system_error>
#include <iostream>

class tagged_error : public std::system_error
{
private:
  std::string attempt_and_error_;

public:
  tagged_error( const std::error_category & category,
                const std::string s_attempt,
                const int error_code )
    : system_error( error_code, category ),
      attempt_and_error_( s_attempt + ": " + std::system_error::what() )
  {}

  const char * what( void ) const noexcept override
  {
    return attempt_and_error_.c_str();
  }
};

class unix_error : public tagged_error
{
public:
  unix_error ( const std::string & s_attempt,
               const int s_errno = errno )
    : tagged_error( std::system_category(), s_attempt, s_errno )
  {}
};

inline void print_exception( const char * argv0, const std::exception & e )
{
  std::cerr << argv0 << ": " << e.what() << std::endl;
}

class internal_error : public std::runtime_error
{
public:
  internal_error( const std::string & s_attempt, const std::string & s_error )
    : runtime_error( s_attempt + ": " + s_error )
  {}
};

class Invalid : public internal_error
{
public:
  Invalid( const std::string & s_error )
    : internal_error( "invalid bitstream", s_error )
  {}
};

class Unsupported : public internal_error
{
public:
  Unsupported( const std::string & s_error )
    : internal_error( "unsupported bitstream", s_error )
  {}
};

class LogicError : public internal_error
{
public:
  LogicError()
    : internal_error( "internal error", "logic error" )
  {}
};

class RPCError : public std::runtime_error
{
public:
  RPCError( const char * what )
    : std::runtime_error( what )
  {}
};

inline int SystemCall( const char * s_attempt, const int return_value )
{
  if ( return_value >= 0 ) {
    return return_value;
  }

  throw unix_error( s_attempt );
}

inline int SystemCall( const std::string & s_attempt, const int return_value )
{
  return SystemCall( s_attempt.c_str(), return_value );
}

template <typename StatusObject>
inline void RPC( const char * s_attempt, const StatusObject status )
{
  if ( not status.ok() ) {
    throw std::runtime_error( s_attempt + std::string( ": " ) + status.error_message() );
  }
}

#endif
