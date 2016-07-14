#include <iostream>
#include <iomanip>

#include "ivf.hh"

using namespace std;

void usage_error( const string & program_name )
{
  cerr << "Usage: " << program_name << " <ivf>" << endl
       << endl;
}

int main( int argc, char const *argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  if ( argc < 2 ) {
    usage_error( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  try {
    IVF ivf( argv[ 1 ] );

    for ( size_t i = 0; i < ivf.frame_count(); i++ ) {
      cout << setfill( '0' ) << setw( 8 ) << i << " " << ivf.frame( i ).size() << endl;
    }

    cout << "Total " << ivf.size() << endl;
  }
  catch ( const exception &  e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
