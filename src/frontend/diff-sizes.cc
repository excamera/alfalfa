#include <iostream>
#include <fstream>
#include "player.hh"
#include "diff_generator.hh"
#include "display.hh"

using namespace std;

int main( int argc, char * argv[] )
{
  if ( argc != 3 ) {
    cerr << "Usage: " << argv[ 0 ] << " SOURCE TARGET" << endl;
    return EXIT_FAILURE;
  }

  FilePlayer<DiffGenerator> source_player( argv[ 1 ] );
  FilePlayer<DiffGenerator> target_player( argv[ 2 ] );

  while ( not source_player.eof() and not target_player.eof() ) {
    source_player.advance();
    target_player.advance();

    SerializedFrame diff = target_player - source_player;

    cout << "Src Frame " << source_player.cur_frame_no() << " Target Frame " << target_player.cur_frame_no() <<
      " original length: " << target_player.original_size() << " Diff length: " << diff.chunk().size() << endl;
  }
    
  return EXIT_SUCCESS;
}
