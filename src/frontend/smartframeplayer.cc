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
#include <boost/graph/graphviz.hpp>

using namespace std;

ostream& operator<<( ostream & out, const FullDecoderHash & hash )
{
  return out << hash.str();
}

struct VertexState
{
  FullDecoderHash cur_decoder;
  unsigned id;
};

struct EdgeState
{
  DecoderHash updater;
  string frame_name;
  size_t size;
};

struct FrameInfo
{
  DecoderHash source;
  DecoderHash target;
  unsigned int size;
};

// listS for VertexList to ensure that Vertex's aren't invalidates
// hash_setS for EdgeList ensures we don't get a multigraph
using FrameGraph = boost::adjacency_list<boost::listS, boost::hash_setS, boost::directedS, VertexState, EdgeState>;
using Vertex = FrameGraph::vertex_descriptor;
using Edge = FrameGraph::edge_descriptor;

class GraphManager
{
private:
  unordered_map<string, FrameInfo> frame_map_;
  FrameGraph graph_;
  vector<Vertex> new_vertices_;
  // Keeping a separate map for the vertices may be less efficient than
  // a custom container that enforces it
  //
  // Hash of FullDecoderHash -> actual vertex
  unordered_map<size_t, Vertex> vertex_map_; //tracks existing vertices;
  unsigned cur_vertex_; // Each vertex needs a unique number

  // Enforces uniqueness
  Vertex add_vertex( const FullDecoderHash & cur_decoder ) {
    size_t vertex_hash = cur_decoder.hash();
    auto v_iter = vertex_map_.find( vertex_hash );
    if ( v_iter != vertex_map_.end() ) {
      return v_iter->second;
    }

    Vertex new_vert = boost::add_vertex( VertexState { cur_decoder, cur_vertex_++ }, graph_ );
    vertex_map_.insert( make_pair( vertex_hash, new_vert ) );

    new_vertices_.push_back( new_vert );

    return new_vert;
  }

  void add_edge( const Vertex & v, const Vertex & u, const EdgeState & state )
  {
    boost::add_edge( v, u, state, graph_ );
  }

  // Read frame info into frame_map_ and populate graph_ with the keyframes
  void read_frames( ifstream & frame_manifest )
  {
    while ( not frame_manifest.eof() ) {
      string frame_name;
      unsigned size;

      frame_manifest >> frame_name >> size;
      
      if ( frame_name == "" ) {
        break;
      }

      DecoderHash source_hash( frame_name, false );
      DecoderHash target_hash( frame_name, true );

      auto inserted = frame_map_.insert( make_pair( frame_name, FrameInfo { source_hash, target_hash, size } ) );

      if ( not inserted.second ) {
        // Come up with the list of starting vertices
        // Check if frame_name is a keyframe
        if ( not source_hash.state().initialized() and
             not source_hash.continuation().initialized() and
             not source_hash.last_ref().initialized() and
             not source_hash.golden_ref().initialized() and
             not source_hash.alt_ref().initialized() and 
             target_hash.state().initialized() and
             target_hash.continuation().initialized() and
             target_hash.last_ref().initialized() and
             target_hash.golden_ref().initialized() and
             target_hash.alt_ref().initialized() ) {

          add_vertex( FullDecoderHash( target_hash.state().get(),
                                       target_hash.continuation().get(),
                                       target_hash.last_ref().get(),
                                       target_hash.golden_ref().get(),
                                       target_hash.alt_ref().get() ) );
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
          const FullDecoderHash & cur_decoder = graph_[ v ].cur_decoder;
          if ( cur_decoder.can_decode( frame_info.source ) ) {
            FullDecoderHash new_state = cur_decoder;
            new_state.update( frame_info.target );


            Vertex new_v = add_vertex( new_state );

            add_edge( v, new_v, EdgeState { frame_info.target, frame_name, frame_info.size } );
          }
        }
      }
    }
  }

  void all_pairs( void )
  {
    //floyd_
  }

public:
  GraphManager( ifstream & frame_manifest )
    : frame_map_(),
      graph_(),
      new_vertices_(),
      vertex_map_(),
      cur_vertex_( 0 )
  {
    read_frames( frame_manifest );
    make_graph();
    all_pairs();
  }

  void dump_graph( const string & graph_name ) const 
  {
    ofstream dot_file( graph_name );

    auto vertex_name = boost::get(&VertexState::cur_decoder, graph_);
    auto edge_name = boost::get(&EdgeState::frame_name, graph_);

    boost::write_graphviz( dot_file, graph_, boost::make_label_writer(vertex_name), boost::make_label_writer( edge_name ), boost::default_writer(), boost::get( &VertexState::id, graph_ ) );
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
  ifstream optional_manifest( "optional_manifest" );

  uint16_t width, height;
  original_manifest >> width >> height;

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

  GraphManager graph( frame_manifest );
  graph.dump_graph( "graph_out.dot" );

  // These factors determine if a path ends of being viable or not
  //long max_buffer_size = stol( argv[ 2 ] );
  //unsigned long byte_rate = stoul( argv [ 3 ] ) / 8;
  //unsigned long delay = stoul( argv[ 4 ] );

}
