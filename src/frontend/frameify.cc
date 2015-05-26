#include <iostream>
#include <fstream>
#include "player.hh"
#include "diff_generator.cc"

using namespace std;

#if 0

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

#endif

void single_switch( ofstream & manifest, unsigned int switch_frame, const string & source, 
		    const string & target, const string & original )
{
  FilePlayer<DiffGenerator> source_player( source );
  FilePlayer<DiffGenerator> target_player( target );
  FilePlayer<DiffGenerator> original_player( original );

  manifest << source_player.width() << " " << source_player.height() << "\n";

  /* Serialize source normally */
  while ( source_player.cur_displayed_frame() < switch_frame ) {
    SerializedFrame frame = source_player.serialize_next();
    manifest << "N " << frame.name() << endl;
    cout << "N " << frame.name() << "\n";
    cout << source_player.cur_frame_stats();
    frame.write();
    if ( source_player.eof() ) {
      throw Invalid( "source ended before switch" );
    }
  }

  unsigned source_frames_size = 0;
  unsigned continuation_frames_size = 0;

  /* At this point we generate the first continuation frame */
  cout << "First Continuation\n";

  /* Catch up target_player */
  while ( target_player.cur_displayed_frame() < switch_frame ) {
    target_player.advance();
  }

  FramePlayer<DiffGenerator> diff_player( source_player );

  SerializedFrame continuation = target_player - diff_player;
  manifest << "C " << continuation.name() << endl;
  continuation.write();
  continuation_frames_size += continuation.size();
  
  cout << "C " << continuation.name() << "\n";

  diff_player.decode( continuation );

  cout << "  Continuation Info\n";
  cout << diff_player.cur_frame_stats();
  cout << "  Corresponding Target Info\n";
  cout << target_player.cur_frame_stats();

  unsigned int last_displayed_frame = target_player.cur_displayed_frame();

  SerializedFrame next_target = target_player.serialize_next();

  while ( not source_player.eof() and not target_player.eof() ) {
    /* Could get rid of the can_decode and just make this a try catch */
    if ( diff_player.can_decode( next_target ) ) {
      manifest << "N " << next_target.name() << endl;
      cout << "N " << next_target.name() << "\n";

      cout << target_player.cur_frame_stats() << "\n";
      next_target.write();

      diff_player.decode( next_target );
    }
    else {
      /* if the cur_displayed_frame is the same then next_target wasn't displayed, so advance */
      if ( target_player.cur_displayed_frame() == last_displayed_frame ) {
	/* PSNR for continuation frames calculated from target raster */
	target_player.advance();
      }

      /* Write out all the source frames up to the position of target_player */
      while ( source_player.cur_displayed_frame() < target_player.cur_displayed_frame() ) {
	/* Don't really care about PSNR of S frames */
	RasterHandle source_raster( source_player.width(), source_player.height() );

	SerializedFrame source_frame = source_player.serialize_next();
	manifest << "S " << source_frame.name() << endl;

	cout << "S " << source_frame.name() << "\n";
	cout << source_player.cur_frame_stats();
      	source_frame.write();
	source_frames_size += source_frame.size();
      }

      diff_player.update_continuation( source_player );

      /* Make another diff */
      continuation = target_player - diff_player;
      continuation.write();
      continuation_frames_size += continuation.size();

      manifest << "C " << continuation.name() << endl;

      // Need to decode _before_ we call cur_frame_stats, 
      // otherwise it will be the stats of the previous frame
      diff_player.decode( continuation );

      cout << "C " << continuation.name() << "\n";
      cout << "  Continuation Info\n";
      cout << diff_player.cur_frame_stats();
      cout << "  Corresponding Target Info\n";
      cout << "   " << next_target.name() << "\n";
      cout << target_player.cur_frame_stats();
    }
    
    last_displayed_frame = target_player.cur_displayed_frame();

    next_target = target_player.serialize_next();
    cout << "Next target name: " << next_target.name() << endl;
  }

  cout << "Total size of S frames: " << source_frames_size << endl;
  cout << "Total size of C frames: " << continuation_frames_size << endl;
  cout << "Total cost of switch: " << source_frames_size + continuation_frames_size << "\n";
}

int main( int argc, char * argv[] )
{
  if ( argc < 3 ) {
    cerr << "Usage: " << argv[ 0 ] << " MANIFEST SWITCH_NO SOURCE TARGET ORIGINAL" << endl;
    return EXIT_FAILURE;
  }

  ofstream manifest( argv[ 1 ] );

  if ( argc == 3 ) {
    //dump_vp8_frames( argv[ 2 ] );
  }
  else if ( argc == 4 ) {
    //generate_diff_frames( argv[ 2 ], argv[ 3 ] );
  }
  else if ( argc == 6 ) {
    single_switch( manifest, stoi( argv[ 2 ] ), argv[ 3 ], argv[ 4 ], argv[ 5 ] );
  }

}
