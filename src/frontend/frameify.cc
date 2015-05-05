#include <iostream>
#include <fstream>
#include "player.hh"
#include "diff_generator.cc"

using namespace std;

void dump_vp8_frames( const string & video_path )
{
  Player player( video_path );

  while ( not player.eof() ) {
    SerializedFrame frame = player.serialize_next();
    frame.write();
  }
}

void generate_diff_frames( const string & source, const string & target )
{
  FilePlayer<DiffGenerator> source_player( source );
  FilePlayer<DiffGenerator> target_player( target );

  while ( not source_player.eof() and not target_player.eof() ) {
    source_player.advance();
    target_player.advance();

    SerializedFrame diff = target_player - source_player;
    diff.write();
  }
}

int main( int argc, char * argv[] )
{
  if ( argc < 3 ) {
    cerr << "Usage: " << argv[ 0 ] << " MANIFEST SOURCE [TARGET]" << endl;
    return EXIT_FAILURE;
  }

  if ( argc == 3 ) {
    dump_vp8_frames( argv[ 2 ] );
  }
  else if ( argc == 4 ) {
    generate_diff_frames( argv[ 2 ], argv[ 3 ] );
  }

}
