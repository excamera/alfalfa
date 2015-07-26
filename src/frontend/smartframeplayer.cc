#include "player.hh"
#include "display.hh"

#include <tuple>
#include <fstream>
#include <utility>
#include <algorithm>
#include <unordered_set>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/functional/hash.hpp>
#include <limits>
#include <unordered_map>

using namespace std;

ostream& operator<<( ostream & out, const DecoderHash & hash )
{
  return out << hash.str();
}

struct VertexState
{
  DecoderHash cur_decoder;
  Optional<size_t> output_hash;
  Optional<double> psnr;
  size_t id;
  unsigned frame_no;
};

struct EdgeState
{
  TargetHash updater;
  string frame_name;
  size_t size; // Size of frame, for use as edge weight
};

struct FrameInfo
{
  SourceHash source;
  TargetHash target;
  Optional<double> psnr;
  unsigned int size;
  string frame_name;
};

static Optional<size_t> get_shown_hash( const TargetHash & target )
{
  return make_optional( target.shown, target.output_hash );
}

// listS for VertexList to ensure that Vertex's aren't invalidates
// hash_setS for EdgeList ensures we don't get a multigraph
using FrameGraph = boost::adjacency_list<boost::listS, boost::listS, boost::bidirectionalS, VertexState, EdgeState>;
using Vertex = FrameGraph::vertex_descriptor;
using Edge = FrameGraph::edge_descriptor;

class GraphManager
{
private:
  FrameGraph graph_;

  // FIXME Keeping a separate map for the vertices may be less efficient than
  // a custom container that enforces it
  //
  // Hash of DecoderHash -> actual vertex
  unordered_map<size_t, Vertex> vertex_map_; //tracks existing vertices;

  vector<Vertex> new_vertices_;
  unsigned cur_vertex_; // Each vertex needs a unique number
  vector<pair<Vertex, string>> starts_;

  size_t num_vertices( void ) const
  {
    return boost::num_vertices( graph_ );
  }

  // Enforces uniqueness
  Vertex add_vertex( const DecoderHash & cur_decoder, unsigned frame_no, const Optional<double> & psnr, const Optional<size_t> & output_hash, bool last ) {
    size_t vertex_hash = cur_decoder.hash();
    boost::hash_combine( vertex_hash, frame_no );

    auto v_iter = vertex_map_.find( vertex_hash );
    if ( v_iter != vertex_map_.end() ) {
      // Some hash collision asserts...
      assert( graph_[ v_iter->second ].frame_no == frame_no );
      assert( graph_[ v_iter->second ].cur_decoder == cur_decoder ); 
      return v_iter->second;
    }

    Vertex new_vert = boost::add_vertex( VertexState { cur_decoder, output_hash, psnr, num_vertices(), frame_no }, graph_ );
    vertex_map_.insert( make_pair( vertex_hash, new_vert ) );

    if ( not last ) {
      new_vertices_.push_back( new_vert );
    }

    return new_vert;
  }

  void add_edge( const Vertex & v, const Vertex & u, const EdgeState & state )
  {
    boost::add_edge( v, u, state, graph_ );
  }

  void cleanup_dead_end( const Vertex & v )
  {
    auto in_edge_iters = boost::in_edges( v, graph_ );

    // Record all the predecessors in the graph
    vector<Vertex> predecessors;
    for ( auto edge_iter = in_edge_iters.first; edge_iter != in_edge_iters.second; edge_iter++ ) {
      predecessors.push_back( boost::source( *edge_iter, graph_ ) );
    }
    
    boost::clear_vertex( v, graph_ );
    boost::remove_vertex( v, graph_ );

    // After removing v if the predecessors have no children remove them too
    for ( Vertex pred : predecessors ) {
      if ( boost::out_degree( pred, graph_ ) == 0 ) {
        cleanup_dead_end( pred );
      }
    }
  }

public:
  GraphManager( const vector<vector<FrameInfo>> & frames )
    : graph_(),
      vertex_map_(),
      new_vertices_(),
      cur_vertex_( 0 ),
      starts_()
  {
    // Find all the keyframes that display the first raster
    // These are the starting vertices
    for ( const FrameInfo & frame : frames[ 0 ] ) {
      const SourceHash & source = frame.source;
      const TargetHash & target = frame.target;

      // Check if frame is a keyframe
      if ( not source.state_hash.initialized() and
           not source.continuation_hash.initialized() and
           not source.last_hash.initialized() and
           not source.golden_hash.initialized() and
           not source.alt_hash.initialized() and 
           target.shown and
           target.update_last and
           target.update_golden and
           target.update_alternate ) {
         Vertex v = add_vertex( DecoderHash( target.state_hash,
                                  target.continuation_hash,
                                  target.output_hash,
                                  target.output_hash,
                                  target.output_hash ),
                     0, frame.psnr, get_shown_hash( target ), false );

         starts_.push_back( make_pair( v, frame.frame_name ) );
      }
    }

    const vector<FrameInfo> & unshown_frames = frames.back();

    // For every newly created vertices check every frame that it can 
    // potentially decode (ie unshown frames or frames that display the proper next raster)
    while ( not new_vertices_.empty() ) {
      vector<Vertex> prev_new_vertices( move( new_vertices_ ) );
      new_vertices_.clear();

      for ( const Vertex v : prev_new_vertices ) {
        const DecoderHash & cur_decoder = graph_[ v ].cur_decoder;
        unsigned cur_frame_no = graph_[ v ].frame_no;
        unsigned next_frame_no = cur_frame_no + 1;

        // If this is left true then v is a dead end
        bool dead = true;
        for ( const FrameInfo & frame : frames[ next_frame_no ] ) {
          if ( cur_decoder.can_decode( frame.source ) ) {
            dead = false; // v can decode something, so it's not a dead end!

            DecoderHash new_state = cur_decoder;
            new_state.update( frame.target );

            // If it's shown advance the frame number
            unsigned new_frame_no = frame.target.shown ? next_frame_no : cur_frame_no;

            // end of the road, so don't put this in new_vertices_
            bool last = new_frame_no == ( frames.size() - 2 );

            Vertex new_v = add_vertex( new_state, new_frame_no, frame.psnr, get_shown_hash( frame.target ), last );

            if ( v != new_v ) {
              add_edge( v, new_v, EdgeState { frame.target, frame.frame_name, frame.size } );
            }
            else {
              // FIXME, this means we have a nop hidden frame which really doesn't make sense? (hash collision?)
              assert( not frame.target.shown ); // If this was a shown frame which produces the same vertex??? (should be impossible as frame_no increments )
            }
          }
        }
        for ( const FrameInfo & frame : unshown_frames ) {
          if ( cur_decoder.can_decode( frame.source ) ) {
            dead = false; // v can decode something, so it's not a dead end!

            DecoderHash new_state = cur_decoder;
            new_state.update( frame.target );

            // If it's shown advance the frame number
            unsigned new_frame_no = frame.target.shown ? next_frame_no : cur_frame_no;

            // end of the road, so don't put this in new_vertices_
            bool last = new_frame_no == ( frames.size() - 2 );

            Vertex new_v = add_vertex( new_state, new_frame_no, frame.psnr, get_shown_hash( frame.target ), last );

            if ( v != new_v ) {
              add_edge( v, new_v, EdgeState { frame.target, frame.frame_name, frame.size } );
            }
            else {
              // FIXME, this means we have a nop hidden frame which really doesn't make sense? (hash collision?)
              assert( not frame.target.shown ); // If this was a shown frame which produces the same vertex??? (should be impossible as frame_no increments )
            }
          }
        }
        if ( dead ) {
          // v is a dead end so let's clean this path up.
          // this way we ensure that every path left in the resultant graph
          // is a valid way to play the video
          cleanup_dead_end( v );
        }
      }
    }
    cout << "Graph has " << num_vertices() << " vertices and " <<
      boost::num_edges( graph_ ) << " edges\n";
  }

  // stupid
  vector<string> get_path( void ) const
  {
    vector<string> path;
    Vertex v = starts_[ 0 ].first;
    path.push_back( starts_[ 0 ].second );
    while ( boost::out_degree( v, graph_ ) > 0 ) {
      auto edge_iters = boost::out_edges( v, graph_ );

      Edge first_edge = *( edge_iters.first );

      path.push_back( graph_[ first_edge ].frame_name );
      v = boost::target( first_edge, graph_ );
    }
    return path;
  }

  void dump_graph( const string & graph_name ) const 
  {
    ofstream dot_file( graph_name );

    auto vertex_name = boost::get( &VertexState::cur_decoder, graph_ );
    auto edge_name = boost::get( &EdgeState::frame_name, graph_ );

    boost::write_graphviz( dot_file, graph_, boost::make_label_writer( vertex_name ), boost::make_label_writer( edge_name ), boost::default_writer(), boost::get( &VertexState::id, graph_ ) );
  }
};

int main( int argc, char * argv[] )
{
  // DELAY
  if ( argc < 5 ) {
    cerr << "Usage: VIDEO_DIR BUFFER_SIZE BIT_RATE DELAY";
    return EXIT_FAILURE;
  }

  if ( chdir( argv[ 1 ] ) ) {
    throw unix_error( "Couldn't chdir into VIDEO_DIR" );
  }

  ifstream original_manifest( "original_manifest" );
  ifstream quality_manifest( "quality_manifest" );
  ifstream frame_manifest( "frame_manifest" );

  uint16_t width, height;
  original_manifest >> width >> height;

  // Map from original hash to the frame numbers that use it
  unordered_map<size_t, vector<unsigned>> orig_to_num;

  unsigned frame_no = 0;
  while ( not original_manifest.eof() ) {
    string original_hash;
    original_manifest >> original_hash;

    if ( original_hash == "" ) {
      // Silly last line edge case
      break;
    }

    orig_to_num[ stoul( original_hash, nullptr, 16 ) ].push_back( frame_no );

    frame_no++;
  }

  // From approximation to a vector psnr and frame number pairs.
  // The key can approximate any of the frame numbers in the vector
  // The vector seems necessarily because two originals could compress
  // to the same approximation?
  unordered_map<size_t, vector<pair<double, unsigned>>> output_quality_map;
  // Read in the quality manifest
  while ( not quality_manifest.eof() ) {
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
    size_t output_hash = stoul( approx, nullptr, 16 );
    size_t original_hash = stoul( original, nullptr, 16 );

    // Record the possible frames this approximation satisfies
    for ( unsigned frame_no : orig_to_num[ original_hash ] ) {
      output_quality_map[ output_hash ].push_back( make_pair( psnr, frame_no ) );
    }
  }

  // This map ensures we don't process duplicate frames, probably could just have
  // frameify deal with the duplicate problem FIXME
  unordered_map<string, tuple<SourceHash, TargetHash, unsigned>> name_to_frame;

  while ( not frame_manifest.eof() ) {
    string frame_name;
    unsigned size;

    frame_manifest >> frame_name >> size;
    
    if ( frame_name == "" ) {
      break;
    }

    name_to_frame.insert( make_pair( frame_name, 
                                     make_tuple( SourceHash( frame_name ),
                                                 TargetHash( frame_name ), size ) ) );
  }

  // The frames vector holds a list of every possible frame for each displayed frame number
  // ( frames[ 0 ] = all frames for the first displayed frame in video )
  // There are orig_to_num total displayed frames, so frames holds originals.size() + 1
  // vectors so unshown frames are at the last index
  vector<vector<FrameInfo>> frames( frame_no + 1 );
  const unsigned unshown_idx = frame_no;

  for ( const auto & frame_pair : name_to_frame ) {
    const SourceHash & source = get<0>( frame_pair.second );
    const TargetHash & target = get<1>( frame_pair.second );
    unsigned size = get<2>( frame_pair.second );

    if ( not target.shown ) {
      FrameInfo frame { source, target, Optional<double>(), size, frame_pair.first };
      frames[ unshown_idx ].push_back( frame );
    }
    else {
      for ( const auto & output_info : output_quality_map.find( target.output_hash )->second ) {
        FrameInfo frame { source, target, make_optional( true, output_info.first ), size, frame_pair.first };
        frames[ output_info.second ].push_back( frame );
      }
    }
  }


  GraphManager graph( frames );
  graph.dump_graph( "graph_out.dot" );
  vector<string> path = graph.get_path();

  cout << "Path is " << path.size() << " frames long\n";

  //FramePlayer player { width, height };
  //VideoDisplay display { player.example_raster() };

  //for ( string frame_name : path ) {
  //  cout << frame_name << "\n";
  //  SerializedFrame frame { frame_name };

  //  Optional<RasterHandle> output = player.decode( frame );
  //  if ( output.initialized() ) {
  //    display.draw( output.get() );
  //  }
  //}


  // These factors determine if a path ends of being viable or not
  //long max_buffer_size = stol( argv[ 2 ] );
  //unsigned long byte_rate = stoul( argv [ 3 ] ) / 8;
  //unsigned long delay = stoul( argv[ 4 ] );

}
