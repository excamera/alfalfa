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
    RasterHandle stream_raster( target_player.width(), target_player.height() );
    SerializedFrame frame = source_player.serialize_next( stream_raster );
    manifest << "N " << frame.name() << "\n";
    cout << "N " << frame.name() << "\n";
    cout << source_player.cur_frame_stats();
    frame.write();
    if ( source_player.eof() ) {
      throw Unsupported( "source ended before switch" );
    }
  }
  
  /* At this point we generate the first continuation frame */

  /* Catch up target_player */
  while ( target_player.cur_displayed_frame() < switch_frame ) {
    target_player.advance();
  }

  FramePlayer<DiffGenerator> diff_player( source_player );

  SerializedFrame continuation = target_player - diff_player;
  manifest << "C " << continuation.name() << "\n";
  continuation.write();
  
  diff_player.decode( continuation );

  cout << "C " << continuation.name() << "\n";
  cout << "  Continuation Info\n";
  cout << diff_player.cur_frame_stats();
  cout << "  Corresponding Target Info\n";
  cout << target_player.cur_frame_stats();

  unsigned int last_displayed_frame = target_player.cur_displayed_frame();

  RasterHandle stream_raster( target_player.width(), target_player.height() );
  SerializedFrame next_target = target_player.serialize_next( stream_raster );

  while ( not source_player.eof() and not target_player.eof() ) {
    /* Could get rid of the can_decode and just make this a try catch */
    if ( diff_player.can_decode( next_target ) ) {
      manifest << "N " << next_target.name() << "\n";
      next_target.write();

      diff_player.decode( next_target );
    }
    else {
      /* next_target wasn't displayed */
      if ( target_player.cur_displayed_frame() == last_displayed_frame ) {
	/* PSNR for continuation frames calculated from target raster */
	stream_raster = target_player.advance();
      }

      /* Write out all the source frames up to the position of target_player */
      while ( source_player.cur_displayed_frame() < target_player.cur_displayed_frame() ) {
	RasterHandle source_raster( source_player.width(), source_player.height() ); /* Don't really care about PSNR of S frames */

	SerializedFrame source_frame = source_player.serialize_next( source_raster ); 
	manifest << "S " << source_frame.name() << "\n";

	cout << "S " << source_frame.name() << "\n";
	cout << source_player.cur_frame_stats();
      	source_frame.write();
      }

      diff_player.update_continuation( source_player );

      /* Make another diff */
      continuation = target_player - diff_player;

      manifest << "C " << continuation.name() << "\n";

      cout << "C " << continuation.name() << "\n";
      cout << "  Continuation Info\n";
      cout << diff_player.cur_frame_stats();
      cout << "  Corresponding Target Info\n";
      cout << "   " << next_target.name() << "\n";
      cout << target_player.cur_frame_stats();

      continuation.write();

      diff_player.decode( continuation );
    }
    
    last_displayed_frame = target_player.cur_displayed_frame();

    next_target = target_player.serialize_next( stream_raster );
  }
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
