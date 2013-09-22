#include "ivf.hh"
#include "file.hh"

using namespace std;

static const int expected_header_len = 32;

IVF::IVF( const string & filename )
try :
  file_( filename ),
    header_( file_.block( 0, expected_header_len ) )
      {
	if ( dkif() != "DKIF" ) {
	  throw Exception( filename, "not an IVF file" );
	}

	if ( version() != 0 ) {
	  throw Exception( filename, "not an IVF version 0 file" );
	}

	if ( header_len() != expected_header_len ) {
	  throw Exception( filename, "unsupported IVF header length" );
	}
      }
catch ( const File::BoundsCheckException & e )
  {
    throw Exception( filename, "not an IVF file" );
  }
