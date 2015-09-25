#include "player.hh"

#include <fstream>

using namespace std;

/* This program uses stream_manifest to traverse the path along a single
 * switch
 */

bool dump_frame_output( FramePlayer & player, const string & frame_name )
{
  Optional<RasterHandle> raster;
  SerializedFrame frame( frame_name );

  raster = player.decode( frame );
  if ( raster.initialized() ) {
    raster.get().get().dump( stdout );
    return true;
  }
  return false;
}

int main( int argc, char * argv[] )
{
  if ( argc < 3 ) {
    cerr << "Usage: VIDEO_DIR SWITCH_NUM" << endl;
    return EXIT_FAILURE;
  }

  if ( chdir( argv[ 1 ] ) ) {
    throw unix_error( "chdir failed" );
  }

  unsigned long switch_num = stoul( argv[ 2 ] );

  ifstream stream_manifest( "stream_manifest" );
  ifstream frame_manifest( "frame_manifest" );
  ifstream original_manifest( "original_manifest" );

  vector<string> continuation_frames;
  while ( not frame_manifest.eof() ) {
    string frame_name;
    frame_manifest >> frame_name;

    if ( frame_name == "" ) {
      break;
    }

    // Decode the source component to see if this is continuation frame
    SourceHash source_hash( frame_name );

    if ( source_hash.continuation_hash.initialized() ) {
      continuation_frames.push_back( frame_name );
    }
  }

  vector<vector<string>> stream_frames( 2 );

  while ( not stream_manifest.eof() ) {
    unsigned stream_idx;
    string frame_name;
    stream_manifest >> stream_idx >> frame_name;

    stream_frames[ stream_idx ].push_back( frame_name );
  }

  uint16_t width, height;
  original_manifest >> width >> height;

  FramePlayer player( width, height );

  // Dump from source stream
  unsigned frame_num = 0;
  unsigned displayed_frame_num = 0;
  while ( displayed_frame_num <= switch_num ) {
    const string & frame_name = stream_frames[ 0 ][ frame_num ];

    if ( dump_frame_output( player, frame_name ) ) {
      displayed_frame_num++;
    }
    frame_num++;
  }

  // Dump continuations
  for ( const string & continuation : continuation_frames ) {
    if ( dump_frame_output( player, continuation ) ) {
      displayed_frame_num++;
    }
  }

  // Dump remainder from target stream
  unsigned target_displayed_num = 0;
  for ( frame_num = 0; target_displayed_num < displayed_frame_num; frame_num++ ) {
    const string & frame_name = stream_frames[ 1 ][ frame_num ];

    if ( TargetHash( frame_name ).shown ) {
      target_displayed_num++;
    }
  }

  for ( ; frame_num < stream_frames[ 1 ].size(); frame_num++ ) {
    const string & frame_name = stream_frames[ 1 ][ frame_num ];

    dump_frame_output( player, frame_name );
  }
}
