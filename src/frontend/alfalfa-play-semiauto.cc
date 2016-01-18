#include <cstdlib>
#include <iostream>

#include <sysexits.h>

using namespace std;

int main( const int argc, char const *argv[] )
{
  if ( argc != 2 ) {
    cerr << "Usage: " << argv[ 0 ] << " <server-address>" << "\n";
    return EX_USAGE;
  }

  const string server_address( argv[ 1 ] );

  return EXIT_SUCCESS;
}
