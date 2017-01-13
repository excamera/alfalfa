
#include <string>
#include <stdexcept>

#include "paranoid.hh"
#include "exception.hh"

unsigned int paranoid::stoul( const std::string & in )
{
  const unsigned int ret = std::stoul( in );
  const std::string roundtrip = std::to_string( ret );

  if ( roundtrip != in ) {
    throw std::runtime_error( "invalid unsigned integer: " + in );
  }

  return ret;
}
