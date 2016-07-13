/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

using namespace std;

#include <iostream>
#include <fstream>

#include "continuation_player.hh"

static void single_switch( ofstream & manifest, unsigned int switch_frame,
                           const string & source, const string & target )
{
  ContinuationPlayer source_player( source );
  ContinuationPlayer target_player( target );

  manifest << source_player.width() << " " << source_player.height() << "\n";

  /* Serialize source normally */
  unsigned source_cur_frame = 0;
  while ( source_cur_frame < switch_frame ) {
    SerializedFrame frame = source_player.serialize_next();
    manifest << frame.name() << endl;
    cout << "N " << frame.name() << "\n";
    cout << "\tSize: " << frame.size() << "\n";
    cout << source_player.get_frame_stats();
    frame.write();
    if ( frame.shown() ) {
      source_cur_frame++;   
    }

    if ( source_player.eof() ) {
      throw Invalid( "source ended before switch" );
    }
  }

  unsigned continuation_frames_size = 0;

  /* At this point we generate the first continuation frame */
  cout << "First Continuation\n";

  /* Catch up target_player to one behind, since we serialize switch_frame */
  unsigned target_cur_frame;
  for ( target_cur_frame = 0; target_cur_frame < switch_frame - 1; target_cur_frame++ ) {
    target_player.advance();
  }

  SourcePlayer diff_player( source_player );

  while ( not target_player.eof() ) {
    SerializedFrame next_target = target_player.serialize_next();

    if ( next_target.shown() ) {
      target_cur_frame++;
    }

    /* Could get rid of the can_decode and just make this a try catch */
    if ( diff_player.can_decode( next_target ) ) {
      manifest << next_target.name() << endl;
      next_target.write();

      cout << "N " << next_target.name() << endl;
      cout << "\tSize: " << next_target.size() << endl;
      cout << target_player.get_frame_stats() << endl;

      diff_player.decode( next_target );
    }
    else {
      /* PSNR for continuation frames calculated from target raster */
      if ( not next_target.shown() ) {
        target_player.advance();
      }

      /* Make another diff */
      SerializedFrame continuation = target_player - diff_player;
      continuation.write();
      continuation_frames_size += continuation.size();

      manifest << continuation.name() << endl;

      // Need to decode _before_ we call cur_frame_stats, 
      // otherwise it will be the stats of the previous frame
      diff_player.decode( continuation );

      cout << "C " << continuation.name() << "\n";
      cout << "  Continuation Info\n";
      cout << "\tSize: " << continuation.size() << endl;
      //FIXME, subclass ContinuationPlayer and print this in operator-
      //cout << diff_player.cur_frame_stats();
      cout << "  Corresponding Target Info\n";
      cout << "\tSize: " << next_target.size() << endl;
      cout << "   " << next_target.name() << "\n";
      cout << target_player.get_frame_stats();
    }
  }

  cout << "Total cost of switch: " << continuation_frames_size << endl;
}

int main( int argc, char * argv[] )
{
  if ( argc < 5 ) {
    cerr << "Usage: " << argv[ 0 ] << " MANIFEST SWITCH_NO SOURCE TARGET" << endl;

    return EXIT_FAILURE;
  }

  ofstream manifest( argv[ 1 ] );
  single_switch( manifest, stoi( argv[ 2 ] ), argv[ 3 ], argv[ 4 ] );

  return 0;
}
