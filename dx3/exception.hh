#ifndef EXCEPTION_HH
#define EXCEPTION_HH

#include <string>
#include <iostream>
#include <cstring>

class Exception
{
private:
  std::string attempt_, error_;

public:
  Exception( const std::string & s_attempt, const std::string & s_error )
    : attempt_( s_attempt ), error_( s_error )
  {}

  Exception( const std::string & s_attempt )
    : Exception( s_attempt, strerror( errno ) )
  {}

  void perror( void ) const
  {
    std::cerr << attempt_ << ": " << error_ << std::endl;
  }
};

static int SystemCall( const std::string & s_attempt, const int return_value )
{
  if ( return_value >= 0 ) {
    return return_value;
  }

  throw Exception( s_attempt );
}

#endif
