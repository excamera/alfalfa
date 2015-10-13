#include <iostream>
#include <fstream>

#include "frame_db.hh"

using namespace std;

int main( int argc, char const *argv[] )
{
  if ( argc < 2 ) {
    cerr << "Usage: framedb-test [frame manifest]" << endl;
    return 1;
  }

  FrameDB fdb( argv[1] );
  fdb.save();

  return 0;
}
