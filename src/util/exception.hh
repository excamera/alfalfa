/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

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
