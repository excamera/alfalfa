#include "subprocess.hh"
#include "exception.hh"

Subprocess::Subprocess()
{}

void Subprocess::call( const std::string & command, const std::string & type )
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

size_t Subprocess::write_string( const std::string & str )
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
