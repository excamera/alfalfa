#include <iostream>
#include <fstream>
#include <unordered_set>
#include "player.hh"
#include "diff_generator.cc"

using namespace std;

namespace std {
  template<>
  struct hash<ReferenceTracker>
  {
    size_t operator()( const ReferenceTracker & refs ) const
    {
      return refs.continuation() | ( refs.last() << 1 ) | ( refs.golden() << 2 ) |
             ( refs.alternate() << 3 );
    }
  };
}

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
      RasterHandle target_raster( MutableRasterHandle( target_player.width(), target_player.height() ) );
      /* if the cur_displayed_frame is the same then next_target wasn't displayed, so advance */
      if ( target_player.cur_displayed_frame() == last_displayed_frame ) {
	/* PSNR for continuation frames calculated from target raster */
	target_raster = target_player.advance();
      }
      else {
        target_raster = next_target.get_output();
      }

      /* Write out all the source frames up to the position of target_player */
      while ( source_player.cur_displayed_frame() < target_player.cur_displayed_frame() ) {
	/* Don't really care about PSNR of S frames */
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
      continuation.set_output( target_raster );
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

static void write_frame( const SerializedFrame & frame, ofstream & frame_manifest )
{
  frame_manifest << frame.name() << " " << frame.size() << endl;
  frame.write();
}

static SerializedFrame serialize_until_shown( FilePlayer<DiffGenerator> & player, ofstream & frame_manifest )
{
  SerializedFrame frame = player.serialize_next(); 

  /* Serialize and write out undisplayed frames */
  while ( not frame.shown() ) {
    write_frame( frame, frame_manifest );
    frame = player.serialize_next();
  }

  write_frame( frame, frame_manifest );

  return frame;
}

static void generate_continuations( const FilePlayer<DiffGenerator> & source_player, 
                                    const FilePlayer<DiffGenerator> & target_player,
                                    const RasterHandle & target_raster, ofstream & frame_manifest, 
                                    const unordered_set<ReferenceTracker> & reference_combinations )
{
  for ( const auto & missing_refs : reference_combinations ) {
    FramePlayer<DiffGenerator> diff_player = source_player;
    diff_player.set_references( not missing_refs.last(), not missing_refs.golden(),
                                not missing_refs.alternate(), target_player );

    SerializedFrame continuation = target_player - diff_player;
    continuation.set_output( target_raster );

    write_frame( continuation, frame_manifest );
  }
}

static unordered_set<ReferenceTracker> new_reference_set( const unordered_set<ReferenceTracker> & refs_set,
                                                          const ReferenceTracker & updated )
{
  unordered_set<ReferenceTracker> new_set;
  for ( auto & refs : refs_set ) {
    ReferenceTracker new_refs = refs;

    if ( updated.last() ) {
      new_refs.set_last( false );
    }

    if ( updated.golden() ) {
      new_refs.set_golden( false );
    }

    if ( updated.alternate() ) {
      new_refs.set_alternate( false );
    }

    /* If any references are still missing add this to the updated set */
    if ( refs.last() or refs.golden() or refs.alternate() ) {
      new_set.insert( new_refs );
    }
  }

  return new_set;
}

static void generate_frames( const string & original, const vector<string> & streams )
{
  ofstream original_manifest( "original_manifest" );
  ofstream quality_manifest( "quality_manifest" );
  ofstream frame_manifest( "frame_manifest" );

  Player original_player( original );

  original_manifest << original_player.width() << " " << original_player.height() << "\n";

  vector<FilePlayer<DiffGenerator>> stream_players;
  stream_players.reserve( streams.size() );

  for ( auto & stream : streams ) {
    stream_players.emplace_back( stream );
  }

  vector<unordered_set<ReferenceTracker>> upgrade_continuations( stream_players.size() - 1 );
  vector<unordered_set<ReferenceTracker>> downgrade_continuations( stream_players.size() - 1 );

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

      // We need to look at missing refs plus updated refs. Insert a new entry from missing_references,
      // then check which refs have been updated in the target and the source. Use the set of updated
      // refs to iterate through and update the missing refs in the vector, throwing out any instances
      // which now have no missing refs
      ReferenceTracker target_updated_refs = target_player.updated_references(); 
      upgrade_continuations[ stream_idx ] = new_reference_set( upgrade_continuations[ stream_idx ], target_updated_refs );

      ReferenceTracker source_updated_refs = source_player.updated_references(); 
      downgrade_continuations[ stream_idx ] = new_reference_set( downgrade_continuations[ stream_idx ], source_updated_refs );

      ReferenceTracker missing_refs = source_player.missing_references( target_player );
      upgrade_continuations[ stream_idx ].insert( missing_refs );
      downgrade_continuations[ stream_idx ].insert( missing_refs );

      generate_continuations( source_player, target_player, target_frame.get_output(), frame_manifest, upgrade_continuations[ stream_idx ] );
      generate_continuations( target_player, source_player, source_frame.get_output(), frame_manifest, downgrade_continuations[ stream_idx ] );
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
    if ( mkdir( argv[ 1 ], 0777 ) ) {
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
