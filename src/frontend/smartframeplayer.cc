#include "player.hh"
#include "display.hh"

#include <unordered_map>
#include <tuple>
#include <fstream>
#include <utility>

using namespace std;

struct FrameInfo {
  string name;
  unsigned size;
  double psnr;

  FrameInfo( string setName, unsigned setSize, unsigned setPSNR )
    : name( setName), size( setSize ), psnr( setPSNR )
  {}
};

class DecoderHash {
  array<string, 5> hashes_;

  array<string, 5> split( const string & frame_name, size_t split_pos ) const
  {
    array<string, 5> components;

    size_t end_pos = frame_name.find( '#', split_pos + 1 );
    
    for ( int i = 0; i < 5; i++ ) {
      size_t old_pos = split_pos + 1;

      split_pos = frame_name.find( '_', old_pos );

      if ( split_pos == string::npos or split_pos >= end_pos ) {
        // Find the next '#', otherwise we would include the whole remainder of the string
        split_pos = end_pos;
      }

      components[ i ] = string( frame_name, old_pos, split_pos - old_pos );
    }

    return components;
  }

  array<string, 5> split_target( const string & frame_name ) const
  {
    return split( frame_name, frame_name.find( '#' ) );
  }

  array<string, 5> split_source( const string & frame_name ) const
  {
    return split( frame_name, -1 );
  }

public:

  /* initialize with the target of a start frame */
  DecoderHash( const string & frame_name )
    : hashes_( split_target( frame_name ) )
  {}

  bool can_decode( const string & frame_name )
  {
    array<string, 5> requirements = split_source( frame_name );

    for (int i = 0; i < 5; i++ ) {
      const string & cur = hashes_[ i ];
      const string & requirement = requirements[ i ];

      if ( requirement != "x" and cur != requirement ) {
        return false;
      }
    }

    return true;
  }

  void update_continuation( const string & frame_name )
  {
    hashes_[ 1 ] = split_target( frame_name )[ 1 ];
  }

  void update( const string & new_frame )
  {
    array<string, 5> new_hashes = split_target( new_frame );
    for ( int i = 0; i < 5; i++ ) {
      if ( new_hashes[ i ] != "x" ) {
        hashes_[ i ] = new_hashes[ i ];
      }
    }
  }

  string dump() const
  {
    return hashes_[ 0 ] + "_" + hashes_[ 1 ] + "_" + hashes_[ 2 ] + "_" + hashes_[ 3 ] + "_" + hashes_[ 4 ];
  }
};

vector<string> find_undisplayed_path( string goal_frame, DecoderHash decoder_hash, vector<string> undisplayed )
{
  for ( unsigned i = 0; i < undisplayed.size(); i++ ) {
    string & frame = undisplayed[ i ];
    cout << "Undisplayed: " << frame << "\n";
    vector<string> path;

    DecoderHash test_hash = decoder_hash;
    if ( test_hash.can_decode( frame ) ) {
      path.push_back( frame );
      test_hash.update( frame );

      if ( test_hash.can_decode( goal_frame ) ) {
        path.push_back( goal_frame );
        return path;
      }
      else {
        vector<string> new_choices = undisplayed;
        new_choices.erase( new_choices.begin() + i );

        vector<string> new_path = find_undisplayed_path( goal_frame, test_hash, new_choices );

        if ( new_path.size() ) {
          path.insert( path.end(), new_path.begin(), new_path.end() );
          return path;
        }
      }
    }
  }

  return {};
}

int main( int argc, char * argv[] )
{
  // DELAY
  if ( argc < 5 ) {
    cerr << "Usage: VIDEO_DIR BUFFER_SIZE BIT_RATE DELAY";
    return EXIT_FAILURE;
  }

  string video_dir( argv[ 1 ] );

  ifstream original_manifest( video_dir + "/original_manifest" );
  ifstream quality_manifest( video_dir + "/quality_manifest" );
  ifstream frame_manifest( video_dir + "/frame_manifest" );
  ifstream optional_manifest( video_dir + "/optional_manifest" );

  uint16_t width, height;
  original_manifest >> width >> height;

  /* Frame name => frame size */
  unordered_map<string, unsigned int> frame_map;

  // FIXME? This assumes there is one original for every approximation.
  // Seems plausible that different originals could reduce down to the same approximation

  /* Approximate raster -> original and the approximation quality */
  unordered_map<string, pair<string, double>> approx_to_orig;

  while (not quality_manifest.eof() ) {
    string original, approx, psnr_string;
    quality_manifest >> original >> approx >> psnr_string;
    if ( original == "" ) {
      break;
    }

    double psnr;
    if ( psnr_string == "INF" ) {
      psnr = INFINITY;
    }
    else {
      psnr = stod( psnr_string );
    }

    approx_to_orig[ approx ] = make_pair<string, double>( move( original ), move( psnr ) );
  }


  /* frame name => frame size */
  unordered_map<string, unsigned> frame_sizes;

  /* Output Hash => possible frames */
  unordered_map<string, vector<string>> output_to_frames;

  while ( not frame_manifest.eof() ) {
    string frame_name;
    unsigned size;

    frame_manifest >> frame_name >> size;
    
    if ( frame_name == "" ) {
      break;
    }

    frame_sizes[ frame_name ] = size;

    size_t output_pos = frame_name.rfind('#');
    string output_hash( frame_name, output_pos + 1 );

    output_to_frames[ output_hash ].push_back( frame_name );
  }

  vector<vector<string>> cq_streams;

  // FIXME at some point this whole manifest should be unnecessary
  while ( not optional_manifest.eof() ) {
    unsigned stream_idx;
    string frame_name;
    optional_manifest >> stream_idx >> frame_name;

    if ( frame_name == "" ) {
      break;
    }

    if ( stream_idx >= cq_streams.size() ) {
      cq_streams.resize( stream_idx + 1 );
    }

    cq_streams[ stream_idx ].push_back( frame_name );
  }

  // Store all the originals in a vector
  vector<string> originals;
  while ( not original_manifest.eof() ) {
    string original_hash;
    original_manifest >> original_hash;

    if ( original_hash == "" ) {
      // Silly last line edge case
      break;
    }

    originals.push_back( original_hash );
  }

  // For now, we're going always just start with the first stream
  unsigned start_stream_idx = 0;

  // These factors determine if a path ends of being viable or not
  long max_buffer_size = stol( argv[ 2 ] );
  unsigned long byte_rate = stoul( argv [ 3 ] ) / 8;
  unsigned long delay = stoul( argv[ 4 ] );

  vector<vector<string>> viable_paths;

  DecoderHash decoder_hash( cq_streams[ start_stream_idx ][ 0 ]);

  // Look at all possible single switch paths
  for ( unsigned switch_frame = 0; switch_frame < originals.size(); switch_frame++ ) {
    bool viable = true;
    vector<string> path;
    long cur_buffer_size = delay * byte_rate;

    /* First just tally up the frames preceding the switch */
    for ( unsigned cur_frame = 0; cur_frame <= switch_frame; cur_frame++ ) {
      unsigned size = 0;
      cur_buffer_size += (1/24)*byte_rate;
      if ( cur_buffer_size > max_buffer_size ) {
        cur_buffer_size = max_buffer_size;
      }

      string frame_name = cq_streams[ start_stream_idx ][ cur_frame ];

      // This means there are undisplayed frames in between
      if ( not decoder_hash.can_decode( frame_name ) ) {
        vector<string> undisplayed_path = find_undisplayed_path( frame_name, decoder_hash, output_to_frames[ "x" ] );
        if ( undisplayed_path.size() ) {
          for ( string & undisplayed : undisplayed_path ) {
            size += frame_sizes[ undisplayed ];
            decoder_hash.update( undisplayed );

            path.push_back( undisplayed );
          }
        }
        else {
          cout << "Couldn't decode source component of path\n";
          cout << "state " << decoder_hash.dump() << "\n";
          for (auto x : path ) { cout << x << "\n"; }
          return 0; 
        }
      }
      else {
        size = frame_sizes[ frame_name ];
        decoder_hash.update( frame_name );

        path.push_back( frame_name );
      }


      cur_buffer_size -= size;

      if ( cur_buffer_size < 0 ) {
        viable = false;
        break;
      }

    }

    if ( !viable ) {
      // Try the next possible path
      continue;
    }

    unsigned source_frame = switch_frame;

    for ( unsigned cur_frame = switch_frame; cur_frame < originals.size(); cur_frame++ ) {
      string frame_name = cq_streams[ start_stream_idx + 1 ][ cur_frame ];
      unsigned size = 0;

      cur_buffer_size += (1/24)*byte_rate;

      if ( cur_buffer_size > max_buffer_size ) { 
        cur_buffer_size = max_buffer_size;
      }

      if ( decoder_hash.can_decode( frame_name ) ) {
        size += frame_sizes[ frame_name ];
        decoder_hash.update( frame_name );

        path.push_back( frame_name );
      }
      else { 
        // Find possible undisplayed frames
        vector<string> undisplayed_path = find_undisplayed_path( frame_name, decoder_hash, output_to_frames[ "x" ] );
        if ( undisplayed_path.size() ) {
          for ( string & undisplayed : undisplayed_path ) {
            size += frame_sizes[ undisplayed ];
            decoder_hash.update( undisplayed );

            path.push_back( undisplayed );
          }
        }
        else {
          // Find a continuation frame

          // Catch up on source frames FIXME, need some way to indicate these go to the source decoder
          // How to track them in path
          while ( source_frame < cur_frame ) {
            // Also need to account for hidden source frames here
            source_frame++;

            string source_frame_name = cq_streams[ start_stream_idx ][ source_frame ];

            decoder_hash.update_continuation( source_frame_name );

            size += frame_sizes[ source_frame_name ];
          }

          bool continuation_found = false;

          string output_hash( frame_name, frame_name.rfind( '#' ) + 1 );

          // Search through all the frames that produce the same output as frame_name
          for ( const string & possible_frame : output_to_frames[ output_hash ] ) {
            if ( decoder_hash.can_decode( possible_frame ) ) {
              size += frame_sizes[ possible_frame ];

              decoder_hash.update( possible_frame );

              path.push_back( possible_frame );

              continuation_found = true;

              break;
            }
          }

          if ( !continuation_found ) {
            cout << "Couldn't find a continuation frame " << frame_name << "\n";
            cout << "Source " << decoder_hash.dump() << "\n";
            for (auto x : path) { cout << x << "\n"; }

            viable = false;
            break;
          }
        }
      }

      cur_buffer_size -= size;

      if ( cur_buffer_size < 0 ) {
        viable = false;
        break;
      }
    }

    if ( viable ) {
      viable_paths.push_back( path );
    }
  }

  double max_psnr = -1;
  vector<string> * best_path = nullptr;

  for ( vector<string> & path : viable_paths ) {
    double psnr = 0;
    for ( string & frame : path ) {
     string output_hash( frame, frame.rfind( '#' ) + 1 );
      psnr += approx_to_orig[ output_hash ].second;
    }

    psnr /= path.size();

    if ( psnr > max_psnr ) { 
      best_path = &path;
      max_psnr = psnr;
    }
  }

  cout << max_psnr << "\n";
  for ( string & frame_name : *best_path ) {
  //  cout << frame_name << "\n";
  }

  // Decode best_path
}
