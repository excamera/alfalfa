#include <iostream>
#include <fstream>
#include "player.hh"
#include "diff_generator.cc"

using namespace std;

void dump_vp8_frames( const string & video_path )
{
  FilePlayer<DiffGenerator> player( video_path );

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

void single_switch( ofstream & manifest, unsigned int switch_frame, const string & source, const string & target )
{
  FilePlayer<DiffGenerator> source_player( source );
  FilePlayer<DiffGenerator> target_player( target );

  manifest << source_player.width() << " " << source_player.height();

  /* Serialize source normally */
  while ( source_player.cur_displayed_frame() < switch_frame ) {
    SerializedFrame frame = source_player.serialize_next();
    manifest << "N " << frame.name() << "\n";
    frame.write();
    if ( source_player.eof() ) {
      throw Unsupported( "source ended before switch" );
    }
  }

  /* Catch up target_player */
  while ( target_player.cur_displayed_frame() < switch_frame ) {
    target_player.advance();
  }

  FramePlayer<DiffGenerator> diff_player( source_player );

  SerializedFrame continuation = target_player - diff_player;
  manifest << "C " << continuation.name() << "\n";
  continuation.write();

  diff_player.decode( continuation );

  unsigned int last_displayed_frame = target_player.cur_displayed_frame();

  SerializedFrame next_target = target_player.serialize_next();

  while ( not source_player.eof() and not target_player.eof() ) {
    /* Could get rid of the can_decode and just make this a try catch */
    if ( diff_player.can_decode( next_target ) ) {
      manifest << "N " << next_target.name() << "\n";
      next_target.write();

      diff_player.decode( next_target );
    }
    else {
      /* Make another diff */
    
      /* next_target wasn't displayed */
      if ( target_player.cur_displayed_frame() == last_displayed_frame ) {
	target_player.advance();
      }

      while ( source_player.cur_displayed_frame() < target_player.cur_displayed_frame() ) {
	  SerializedFrame source_frame = source_player.serialize_next(); 
	  manifest << "S " << source_frame.name() << "\n";
      	  source_frame.write();
      }

      diff_player.update_continuation( source_player );
    
      continuation = target_player - diff_player;
      manifest << "C " << continuation.name() << "\n";
      continuation.write();

      diff_player.decode( continuation );
    
    }
    
    last_displayed_frame = target_player.cur_displayed_frame();
    next_target = target_player.serialize_next();
  }
}

# if 0
void single_switch( ofstream & manifest, unsigned int switch_frame, const string & source, const string & target )
{
  FilePlayer<DiffGenerator> source_player( source );
  FilePlayer<DiffGenerator> target_player( target );

  while ( not source_player.eof() and not target_player.eof() ) {
    if ( source_player.cur_displayed_frame() < switch_frame ) {
      SerializedFrame frame = source_player.serialize_next();
      manifest << "N " << frame.name() << "\n";
      frame.write();
    }
    else {
      if ( source_player.cur_displayed_frame() == switch_frame ) {
	/* Catch up target_player */
      	while ( target_player.cur_displayed_frame() < switch_frame ) {
      	  target_player.advance();
      	}
      	SerializedFrame frame = target_player - source_player;
	manifest << "C " << frame.name() << "\n";
      	frame.write();

      	FramePlayer<DiffGenerator> diff_player( source_player );
      	diff_player.decode( frame );

      	/* Generate Diffs until we reach the target set of references */
      	while ( not target_player.equal_references( diff_player ) ) {
      	  unsigned int old_displayed_no = source_player.cur_displayed_frame();
      	  while ( old_displayed_no == source_player.cur_displayed_frame() ) {
      	    // Need some way to signify that we are forking the decoding process
      	    frame = source_player.serialize_next(); 
	    manifest << "S " << frame.name() << "\n";
      	    frame.write();
      	  }

      	  diff_player.update_continuation( source_player );
      	  target_player.advance();

      	  frame = target_player - diff_player;
	  manifest << "C " << frame.name() << "\n";
      	  frame.write();

      	  diff_player.decode( frame );

	  if ( target_player.eof() ) {
	    return;
	  }
      	}
      }
      SerializedFrame frame = target_player.serialize_next();
      manifest << "N " << frame.name() << "\n";
      frame.write();
    }
  }
}
#endif

int main( int argc, char * argv[] )
{
  if ( argc < 3 ) {
    cerr << "Usage: " << argv[ 0 ] << " MANIFEST SOURCE [TARGET]" << endl;
    return EXIT_FAILURE;
  }

  ofstream manifest( argv[ 1 ] );

  if ( argc == 3 ) {
    dump_vp8_frames( argv[ 2 ] );
  }
  else if ( argc == 4 ) {
    generate_diff_frames( argv[ 2 ], argv[ 3 ] );
  }
  else if ( argc == 5 ) {
    single_switch( manifest, stoi( argv[ 2 ] ), argv[ 3 ], argv[ 4 ]);
  }

}
