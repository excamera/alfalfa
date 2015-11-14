#include "subprocess.hh"
#include "exception.hh"

using namespace std;

Subprocess::Subprocess()
{}

void Subprocess::call( const string & command, const string & type )
{
  file_.reset( popen( command.c_str(), type.c_str() ) );

  if ( file_ == nullptr ) {
    throw unix_error( "popen failed" );
  }
}

int Subprocess::wait()
{
  if ( file_ == nullptr ) {
    throw unix_error( "nothing to close" );
  }

  int ret = SystemCall( "pclose", pclose( file_.get() ) );
  file_.release();

  return ret;
}

size_t Subprocess::write_string( const string & str )
{
  if ( file_ == nullptr ) {
    throw unix_error( "nowhere to write" );
  }

  return SystemCall( "fwrite", fwrite( str.c_str(), 1, str.length(), file_.get() ) );
}

Subprocess::~Subprocess()
{
  if ( file_ != nullptr ) {
    pclose( file_.get() );
    file_.release();
  }
}
