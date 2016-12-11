#ifndef UTIL_HH
#define UTIL_HH

#include <system_error>
#include <iostream>
#include <string>
#include <cstring>

/* tagged_error: system_error + name of what was being attempted */
class tagged_error : public std::system_error
{
private:
  std::string attempt_and_error_;

public:
  tagged_error( const std::error_category & category,
		const std::string & s_attempt,
		const int error_code )
    : system_error( error_code, category ),
      attempt_and_error_( s_attempt + ": " + std::system_error::what() )
  {}

  const char * what( void ) const noexcept override
  {
    return attempt_and_error_.c_str();
  }
};

/* unix_error: a tagged_error for syscalls */
class unix_error : public tagged_error
{
public:
  unix_error( const std::string & s_attempt,
	      const int s_errno = errno )
    : tagged_error( std::system_category(), s_attempt, s_errno )
  {}
};

/* print the error to the terminal */
inline void print_exception( const std::exception & e )
{
    std::cerr << e.what() << std::endl;
}

/* error-checking wrapper for most syscalls */
inline int SystemCall( const char * s_attempt, const int return_value )
{
  if ( return_value >= 0 ) {
    return return_value;
  }

  throw unix_error( s_attempt );
}

/* version of SystemCall that takes a C++ std::string */
inline int SystemCall( const std::string & s_attempt, const int return_value )
{
  return SystemCall( s_attempt.c_str(), return_value );
}

/* zero out an arbitrary structure */
template <typename T> void zero( T & x ) { memset( &x, 0, sizeof( x ) ); }

#endif /* UTIL_HH */
