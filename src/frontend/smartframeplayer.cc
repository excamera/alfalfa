#include "player.hh"
#include "display.hh"

#include <tuple>
#include <fstream>
#include <utility>
#include <algorithm>
#include <unordered_set>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/floyd_warshall_shortest.hpp>
#include <boost/graph/johnson_all_pairs_shortest.hpp>
#include <boost/graph/graphviz.hpp>
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
};

struct EdgeState
{
  TargetHash updater;
  string frame_name;
  size_t size;
};

struct FrameInfo
{
  SourceHash source;
  TargetHash target;
  unsigned int size;
};

static Optional<size_t> get_shown_hash( const TargetHash & target )
{
  return make_optional( target.shown, target.output_hash );
}

// listS for VertexList to ensure that Vertex's aren't invalidates
// hash_setS for EdgeList ensures we don't get a multigraph
using FrameGraph = boost::adjacency_list<boost::listS, boost::hash_setS, boost::bidirectionalS, VertexState, EdgeState>;
using Vertex = FrameGraph::vertex_descriptor;
using Edge = FrameGraph::edge_descriptor;

class GraphManager
{
private:
  unordered_map<string, FrameInfo> frame_map_;
  unordered_map<size_t, vector<Vertex>> output_to_vertices_;
  unordered_map<size_t, vector<size_t>> orig_to_approx_;
  unordered_map<size_t, double> output_to_psnr_;

  FrameGraph graph_;
  vector<Vertex> new_vertices_;
  // FIXME Keeping a separate map for the vertices may be less efficient than
  // a custom container that enforces it
  //
  // Hash of DecoderHash -> actual vertex
  unordered_map<size_t, Vertex> vertex_map_; //tracks existing vertices;
  unsigned cur_vertex_; // Each vertex needs a unique number

  vector<vector<size_t>> distances_;

  // Floyd needs matrix of vertices
  //unordered_map<Vertex, unordered_map<Vertex, size_t>> distances_;
  //
  size_t num_vertices( void ) const
  {
    return boost::num_vertices( graph_ );
  }

  // Enforces uniqueness
  Vertex add_vertex( const DecoderHash & cur_decoder, Optional<size_t> output_hash ) {
    size_t vertex_hash = cur_decoder.hash();
    auto v_iter = vertex_map_.find( vertex_hash );
    Optional<double> psnr {};
    if ( v_iter != vertex_map_.end() ) {
      if ( output_hash.initialized() ) {
        output_to_vertices_[ output_hash.get() ].push_back( v_iter->second );
        psnr.initialize( output_to_psnr_[ output_hash.get() ] );
      }
      return v_iter->second;
    }

    Vertex new_vert = boost::add_vertex( VertexState { cur_decoder, output_hash, psnr, num_vertices() }, graph_ );
    vertex_map_.insert( make_pair( vertex_hash, new_vert ) );

    new_vertices_.push_back( new_vert );

    if ( output_hash.initialized() ) {
      output_to_vertices_[ output_hash.get() ].push_back( new_vert );
    }

    return new_vert;
  }

  void add_edge( const Vertex & v, const Vertex & u, const EdgeState & state )
  {
    boost::add_edge( v, u, state, graph_ );
  }

  // Read frame info into frame_map_ and populate graph_ with the keyframes
  void read_frames( ifstream & frame_manifest, ifstream & quality_manifest )
  {
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

      size_t original_hash = stoul( original, nullptr, 16 );
      size_t output_hash = stoul( approx, nullptr, 16 );

      output_to_psnr_[ output_hash ] = psnr;
      orig_to_approx_[ original_hash ].push_back( output_hash );
    }

    while ( not frame_manifest.eof() ) {
      string frame_name;
      unsigned size;

      frame_manifest >> frame_name >> size;
      
      if ( frame_name == "" ) {
        break;
      }

      SourceHash source_hash( frame_name );
      TargetHash target_hash( frame_name );

      auto inserted = frame_map_.insert( make_pair( frame_name, FrameInfo { source_hash, target_hash, size } ) );

      if ( not inserted.second ) {
        // Come up with the list of starting vertices
        // Check if frame_name is a keyframe
        if ( not source_hash.state_hash.initialized() and
             not source_hash.continuation_hash.initialized() and
             not source_hash.last_hash.initialized() and
             not source_hash.golden_hash.initialized() and
             not source_hash.alt_hash.initialized() and 
             target_hash.update_last and
             target_hash.update_golden and
             target_hash.update_alternate ) {

          add_vertex( DecoderHash( target_hash.state_hash,
                                   target_hash.continuation_hash,
                                   target_hash.output_hash,
                                   target_hash.output_hash,
                                   target_hash.output_hash ),
                      get_shown_hash( target_hash ) );
        }
      }
    }
  }

  void make_graph( void )
  {
    while ( not new_vertices_.empty() ) {
      vector<Vertex> prev_new_vertices( move( new_vertices_ ) );
      new_vertices_.clear();

      for ( auto frame : frame_map_ ) {
        const string & frame_name = frame.first;
        const FrameInfo & frame_info = frame.second;

        for ( const Vertex v : prev_new_vertices ) {
          const DecoderHash & cur_decoder = graph_[ v ].cur_decoder;
          if ( cur_decoder.can_decode( frame_info.source ) ) {
            DecoderHash new_state = cur_decoder;
            new_state.update( frame_info.target );


            Vertex new_v = add_vertex( new_state, get_shown_hash( frame_info.target ) );

            add_edge( v, new_v, EdgeState { frame_info.target, frame_name, frame_info.size } );
          }
        }
      }
    }

    cout << "Graph has " << num_vertices() << " vertices\n";
  }

  void all_pairs( void )
  {
    // Very slow...
    //boost::floyd_warshall_all_pairs_shortest_paths( graph_, distances_, 
    //    boost::weight_map( boost::get( &EdgeState::size, graph_ ) ) );
    
    distances_.resize( num_vertices(), vector<size_t>( num_vertices() ) );
    boost::johnson_all_pairs_shortest_paths( graph_, distances_, 
        boost::weight_map( boost::get( &EdgeState::size, graph_ ) ).vertex_index_map( boost::get( &VertexState::id, graph_ ) ) );
  }


public:
  GraphManager( ifstream & frame_manifest, ifstream & optional_manifest )
    : frame_map_(),
      output_to_vertices_(),
      orig_to_approx_(),
      output_to_psnr_(),
      graph_(),
      new_vertices_(),
      vertex_map_(),
      cur_vertex_( 0 ),
      distances_()
  {
    read_frames( frame_manifest, optional_manifest );
    make_graph();
    all_pairs();
  }

  void dump_graph( const string & graph_name ) const 
  {
    ofstream dot_file( graph_name );

    auto vertex_name = boost::get( &VertexState::cur_decoder, graph_ );
    auto edge_name = boost::get( &EdgeState::frame_name, graph_ );

    boost::write_graphviz( dot_file, graph_, boost::make_label_writer( vertex_name ), boost::make_label_writer( edge_name ), boost::default_writer(), boost::get( &VertexState::id, graph_ ) );
  }

  vector<Vertex> get_vertices_for_outputs( const vector<size_t> & approxs ) const
  {
    vector<Vertex> vertices;
    for ( size_t approx_hash : approxs ) {

      auto verts = output_to_vertices_.find( approx_hash );

      for ( const Vertex & v : verts->second ) {
        vertices.push_back( v );
      }
    }
    return vertices;
  }

  vector<vector<Vertex>> find_output_paths( const vector<size_t> & originals ) const
  {
    vector<vector<Vertex>> frame_vertices( originals.size() );

    size_t orig = originals.back();

    // Start out with the list of vertices for the final output
    auto last_outputs = orig_to_approx_.find( orig );
    frame_vertices.back() = get_vertices_for_outputs( last_outputs->second );
    
    // Work backwards through the list of originals, skipping the final output
    for ( int idx = originals.size() - 2; idx >= 0; idx-- ) {
      vector<Vertex> viables;
      orig = originals[ idx ];

      // only select vertices that can reach the next output frame

      auto outputs = orig_to_approx_.find( orig );
      for ( Vertex potential : get_vertices_for_outputs( outputs->second ) ) {
        size_t p_id = graph_[ potential ].id;
        bool found = false;
        for ( Vertex next : frame_vertices[ idx + 1 ] ) {
          size_t next_id = graph_[ next ].id;
          if ( distances_[ p_id ][ next_id ] != numeric_limits<size_t>::max() ) {
            viables.push_back( potential );
            found = true;
            break;
          }
        }
        if ( found ) {
          break;
        }
      }

      if ( viables.size() == 0 ) {
        throw LogicError();
      }

      frame_vertices[ idx ] = viables;
    }

    return frame_vertices;
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

  // Store all the originals in a vector
  vector<size_t> original_hashes;
  while ( not original_manifest.eof() ) {
    string original_hash;
    original_manifest >> original_hash;

    if ( original_hash == "" ) {
      // Silly last line edge case
      break;
    }

    original_hashes.push_back( stoul( original_hash, nullptr, 16 ) );
  }

  GraphManager graph( frame_manifest, quality_manifest );
  graph.dump_graph( "graph_out.dot" );
  vector<vector<Vertex>> verts = graph.find_output_paths( original_hashes );

  for ( auto v : verts ) {
    for ( auto u : v ) {
      cout << u << "\n";
    }
  }

  // These factors determine if a path ends of being viable or not
  //long max_buffer_size = stol( argv[ 2 ] );
  //unsigned long byte_rate = stoul( argv [ 3 ] ) / 8;
  //unsigned long delay = stoul( argv[ 4 ] );

}
