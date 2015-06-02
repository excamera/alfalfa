#include <iostream>
#include <fstream>
#include "player.hh"
#include "diff_generator.cc"

using namespace std;

static void single_switch( ofstream & manifest, unsigned int switch_frame, const string & source, 
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
    cout << "\tSize: " << frame.size() << "\n";
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

  /* Catch up target_player to one behind, since we serialize switch_frame */
  while ( target_player.cur_displayed_frame() < switch_frame - 1 ) {
    target_player.advance();
  }

  FramePlayer<DiffGenerator> diff_player( source_player );


  while ( not source_player.eof() and not target_player.eof() ) {
    unsigned int last_displayed_frame = target_player.cur_displayed_frame();
    SerializedFrame next_target = target_player.serialize_next();

    /* Could get rid of the can_decode and just make this a try catch */
    if ( diff_player.can_decode( next_target ) ) {
      manifest << "N " << next_target.name() << endl;
      next_target.write();

      cout << "N " << next_target.name() << endl;
      cout << "\tSize: " << next_target.size() << endl;
      cout << target_player.cur_frame_stats() << endl;

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
      	source_frame.write();

	source_frames_size += source_frame.size();

	cout << "S " << source_frame.name() << endl;
	cout << "\tSize: " << source_frame.size() << endl;
	cout << source_player.cur_frame_stats();
      }

      diff_player.update_continuation( source_player );

      /* Make another diff */
      SerializedFrame continuation = target_player - diff_player;
      continuation.write();
      continuation_frames_size += continuation.size();

      manifest << "C " << continuation.name() << endl;

      // Need to decode _before_ we call cur_frame_stats, 
      // otherwise it will be the stats of the previous frame
      diff_player.decode( continuation );

      cout << "C " << continuation.name() << "\n";
      cout << "  Continuation Info\n";
      cout << "\tSize: " << continuation.size() << endl;
      cout << diff_player.cur_frame_stats();
      cout << "  Corresponding Target Info\n";
      cout << "\tSize: " << next_target.size() << endl;
      cout << "   " << next_target.name() << "\n";
      cout << target_player.cur_frame_stats();
    }
  }

  cout << "Total size of S frames: " << source_frames_size << endl;
  cout << "Total size of C frames: " << continuation_frames_size << endl;
  cout << "Total cost of switch: " << source_frames_size + continuation_frames_size << "\n";
}

static void write_quality_manifest( ofstream & manifest, const SerializedFrame & frame, 
                                    const RasterHandle & original )
{
  manifest << hex << uppercase << original.get().hash() << " " << 
              frame.get_output().get().hash() << " " << frame.psnr( original ) << endl;
}

static SerializedFrame serialize_until_shown( FilePlayer<DiffGenerator> & player, ofstream & frame_manifest )
{
  SerializedFrame frame = player.serialize_next(); 

  /* Serialize and write out undisplayed frames */
  while ( not frame.shown() ) {
    frame_manifest << frame.name() << endl;
    frame.write();
    frame = player.serialize_next();
  }

  frame_manifest << frame.name();
  frame.write();
  return frame;
}

static void generate_continuations( const FilePlayer<DiffGenerator> & source_player, const FilePlayer<DiffGenerator> & target_player,
                                    const RasterHandle & target_raster, ofstream & frame_manifest )
{
  /* At the very least we should only generate combinations that will produce different continuations
   * ie if target only needs last, making different ones with gold on and off will make no difference */
  bool reference_combinations[][ 3 ] = { { false, false, false }, { false, false, true }, { false, true, false },
                                      { true, false, false }, { false, true, true }, { true, false, true },
                                      { true, true, false } };

  for ( auto missing_refs : reference_combinations ) {
    FramePlayer<DiffGenerator> diff_player = source_player;
    diff_player.set_references( missing_refs[ 0 ], missing_refs[ 1 ], missing_refs[ 2 ], target_player );

    SerializedFrame continuation = target_player - diff_player;
    continuation.set_output( target_raster );

    frame_manifest << continuation.name() << endl;
    continuation.write();
  }

}

static void generate_frames( const string & original, const vector<string> & streams )
{
  ofstream original_manifest( "original_manifest" );
  ofstream quality_manifest( "quality_manifest" );
  ofstream frame_manifest( "frame_manifest" );

  Player original_player( original );

  vector<FilePlayer<DiffGenerator>> stream_players;
  stream_players.reserve( streams.size() );

  for ( auto & stream : streams ) {
    stream_players.emplace_back( stream );
  }

  while ( not original_player.eof() ) {
    RasterHandle original_raster = original_player.advance();
    original_manifest << uppercase << hex << original_raster.get().hash() << endl;

    /* For every quality level, write out the normal frames and generate diffs */
    for ( unsigned stream_idx = 0; stream_idx < stream_players.size() - 1; stream_idx++ ) {
      FilePlayer<DiffGenerator> & source_player = stream_players[ stream_idx ];
      FilePlayer<DiffGenerator> & target_player = stream_players[ stream_idx + 1 ];
      assert( not target_player.eof() and not source_player.eof() );

      SerializedFrame source_frame = serialize_until_shown( source_player, frame_manifest );
      write_quality_manifest( quality_manifest, source_frame, original_raster );

      SerializedFrame target_frame = serialize_until_shown( target_player, frame_manifest );
      write_quality_manifest( quality_manifest, target_frame, original_raster );

      generate_continuations( source_player, target_player, target_frame.get_output(), frame_manifest );
      generate_continuations( target_player, source_player, source_frame.get_output(), frame_manifest );
    }
  }
}

int main( int argc, char * argv[] )
{
  if ( argc < 3 ) {
    cerr << "Multi switch usage: " << argv[ 0 ] << " OUTPUT_DIR ORIGINAL STREAM1 STREAM2" << endl;
    cerr << "Single switch usage: " << argv[ 0 ] << " MANIFEST SWITCH_NO SOURCE TARGET ORIGINAL" << endl;
    return EXIT_FAILURE;
  }

  if ( argc == 5 ) {
    if ( mkdir( argv[ 1 ], 0644 ) ) {
      throw unix_error( "mkdir failed" );
    }
    if ( chdir( argv[ 1 ] ) ) {
      throw unix_error( "chdir failed" );
    }
    generate_frames( argv[ 2 ], { argv[ 3 ], argv[ 4 ] } );
  }
  else if ( argc == 6 ) {
    ofstream manifest( argv[ 1 ] );
    single_switch( manifest, stoi( argv[ 2 ] ), argv[ 3 ], argv[ 4 ], argv[ 5 ] );
  }

}
