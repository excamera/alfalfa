/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <cassert>

#include "temp_file.hh"
#include "exception.hh"

using namespace std;

vector<char> to_mutable( const string & str )
{
  vector< char > ret;
  for ( const auto & ch : str ) {
    ret.push_back( ch );
  }
  ret.push_back( 0 ); /* null terminate */

  return ret;
}

UniqueFile::UniqueFile( const string & filename_template )
  : mutable_temp_filename_( to_mutable( filename_template + ".XXXXXX" ) ),
   fd_( SystemCall( "mkstemp", mkstemp( &mutable_temp_filename_[ 0 ] ) ) ),
   moved_away_( false )
{
}

/* unlike UniqueFile, a TempFile is deleted when object destroyed */
TempFile::~TempFile()
{
  if ( moved_away_ ) { return; }

  try {
    SystemCall( "unlink " + name(), unlink( name().c_str() ) );
  } catch ( const exception & e ) {
    print_exception( "tempfile", e );
  }
}

void UniqueFile::write( const string & contents )
{
  assert( not moved_away_ );

  fd_.write( contents );
}

UniqueFile::UniqueFile( UniqueFile && other )
  : mutable_temp_filename_( other.mutable_temp_filename_ ),
   fd_( move( other.fd_ ) ),
   moved_away_( false )
{
  other.moved_away_ = true;
}

string UniqueFile::name( void ) const
{
  assert( mutable_temp_filename_.size() > 1 );
  return string( mutable_temp_filename_.begin(), mutable_temp_filename_.end() - 1 );
}
