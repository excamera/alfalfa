#include "player.hh"
#include "diff_generator.hh"
#include "display.hh"
#include <cstdio>

using namespace std;

int main( int argc, char * argv[] )
{
  if ( argc != 3 ) {
    cerr << "Usage: " << argv[ 0 ] << " SOURCE TARGET" << endl;
    return EXIT_FAILURE;
  }

  FilePlayer<DiffGenerator> source_player( argv[ 1 ] );
  FilePlayer<DiffGenerator> target_player( argv[ 2 ] );

  while ( !source_player.eof() && !target_player.eof() ) {
    source_player.advance();
    target_player.advance();

    vector< uint8_t > diff = target_player - source_player;

    fprintf( stderr, "Src Frame %u, ", source_player.cur_frame_no() );
    fprintf( stderr, "Frame %u, original length: %lu bytes. Diff length: %lu bytes.\n",
	     target_player.cur_frame_no(), target_player.original_size(), diff.size() );

  }
    
  return EXIT_SUCCESS;
}
