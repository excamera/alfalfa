#include <iostream>
#include <string>

#include "frame.hh"

using namespace std;

int main( int argc, char *argv[] )
{
  try {
    if ( argc != 1 ) {
      cerr << "Usage: " << argv[ 0 ] << endl;
      return EXIT_FAILURE;
    }

    BoolDecoder data { { nullptr, 0 } };
    KeyFrame myframe { true, 1024, 1024, data };
    myframe.parse_macroblock_headers( data, ProbabilityTables {} );

    vector<uint8_t> output = myframe.serialize( ProbabilityTables {} );
    for ( const auto & x : output ) {
      cout << x;
    }

  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
