#include <cassert>

#include "subprocess.hh"
#include "exception.hh"

using namespace std;

template <typename T>
T * notnull( const string & attempt, T * x )
{
  if ( x ) { return x; }
  throw unix_error( attempt );
}

Subprocess::Subprocess( const string & command, const string & type )
  : file_( notnull( "popen", popen( command.c_str(), type.c_str() ) ) )
{}

void Subprocess::Deleter::operator()( FILE * x ) const
{
  SystemCall( "pclose", pclose( x ) );
}

void Subprocess::write( const Chunk & chunk )
{
  assert( file_ );
  if ( 1 != fwrite( chunk.buffer(), chunk.size(), 1, file_.get() ) ) {
    throw runtime_error( "fwrite returned short write" );
  }
}

void Subprocess::close()
{
  file_.reset();
  assert( not file_ );
}
