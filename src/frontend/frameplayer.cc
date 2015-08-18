#include "player.hh"
#include "display.hh"

#include <tuple>
#include <fstream>
#include <utility>
#include <algorithm>
#include <set>
#include <unordered_set>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/functional/hash.hpp>
#include <boost/graph/r_c_shortest_paths.hpp>
#include <limits>
#include <unordered_map>
#include <ctime>

using namespace std;

ostream& operator<<( ostream & out, const DecoderHash & hash )
{
  return out << hash.str();
}

struct FrameInfo
{
  SourceHash source;
  TargetHash target;
  Optional<double> quality;
  unsigned int size;
  string name;
};

struct VertexState
{
  DecoderHash cur_decoder;
  unsigned frame_no;
  size_t id;
};

struct EdgeState
{
  const FrameInfo * frame;
  Optional<double> error; // Quality of image, cached for use by rc_shortest
  size_t size; // Size of frame, cached for use as edge weight
  size_t id;
};

class FrameManager
{
private:
  // This stores all the actual FrameInfo's, everything else points to it
  list<FrameInfo> all_frames_;

  vector<set<FrameInfo *>> frames_by_number_;
  unordered_map<size_t, set<FrameInfo *>> frames_by_state_;
  unordered_map<size_t, set<FrameInfo *>> frames_by_continuation_;
  unordered_map<size_t, set<FrameInfo *>> frames_by_last_;
  unordered_map<size_t, set<FrameInfo *>> frames_by_golden_;
  unordered_map<size_t, set<FrameInfo *>> frames_by_alternate_;

  void index_frames() {
    // FIXME we use index 0 for "x" but could that theoretically be hashed to?
    //for (const FrameInfo & frame : all_frames_ ) {
    //  const SourceHash & source = frame.source;
    //  frames_by_state_[ source.state.get_or(0) ].insert( &frame );
    //  frames_by_continuation_[ source.continuation.get_or(0) ].insert( &frame );
    //  frames_by_last_[ source.last.get_or(0) ].insert( &frame );
    //  frames_by_golden_[ source.golden.get_or(0) ].insert( &frame );
    //  frames_by_alternate_[ source.alternate.get_or(0) ].insert( &frame );
    //}
  }
public:
  FrameManager( ifstream & original_manifest, ifstream & quality_manifest, ifstream & frame_manifest )
    : all_frames_(), frames_by_number_(), frames_by_state_(), frames_by_continuation_(),
      frames_by_last_(), frames_by_golden_(), frames_by_alternate_()
  {
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

    // From approximation to a vector quality and frame number pairs.
    // The key can approximate any of the frame numbers in the vector
    // The vector seems necessarily because two originals could compress
    // to the same approximation?
    unordered_map<size_t, vector<pair<double, unsigned>>> output_quality_map;
    // Read in the quality manifest
    while ( not quality_manifest.eof() ) {
      string original, approx, quality_string;
      quality_manifest >> original >> approx >> quality_string;
      if ( original == "" ) {
        break;
      }

      double quality;
      if ( quality_string == "INF" ) {
        quality = INFINITY;
      }
      else {
        quality = stod( quality_string );
      }
      size_t output_hash = stoul( approx, nullptr, 16 );
      size_t original_hash = stoul( original, nullptr, 16 );

      // Record the possible frames this approximation satisfies
      for ( unsigned frame_no : orig_to_num[ original_hash ] ) {
        output_quality_map[ output_hash ].push_back( make_pair( quality, frame_no ) );
      }
    }

    // The frames_by_number_ vector holds a list of every possible frame for each displayed frame number
    // ( frames[ 0 ] = all frames for the first displayed frame in video )
    // There are orig_to_num total displayed frames, so frames holds originals.size() + 1
    // vectors so unshown frames are at the last index
    frames_by_number_.resize( frame_no + 1 );
    const unsigned unshown_idx = frame_no;

    while ( not frame_manifest.eof() ) {
      string frame_name;
      unsigned size;

      frame_manifest >> frame_name >> size;
      
      if ( frame_name == "" ) {
        break;
      }

      SourceHash source( frame_name );
      TargetHash target( frame_name );

      if ( not target.shown ) {
        FrameInfo frame { source, target, Optional<double>(), size, frame_name };
        all_frames_.push_back( frame );
        frames_by_number_[ unshown_idx ].insert( &all_frames_.back() );
      }
      else {
        // For each possible output
        for ( const auto & output_info : output_quality_map.find( target.output_hash )->second ) {
          FrameInfo frame { source, target, make_optional( true, output_info.first ), size, frame_name };
          all_frames_.push_back( frame );
          frames_by_number_[ output_info.second ].insert( &all_frames_.back() );
        }
      }
    }
    index_frames();
  }

  unsigned num_frames( void ) const
  {
    return frames_by_number_.size() - 2;
  }

  vector<const FrameInfo *> start_frames() const
  {
    vector<const FrameInfo *> starts;
    // Find all the keyframes that display the first raster
    // These are the starting vertices
    for ( const FrameInfo * frame : frames_by_number_[ 0 ] ) {
      const SourceHash & source = frame->source;
      const TargetHash & target = frame->target;

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
        starts.push_back( frame );
      }
    }
    return starts;
  }

  vector<const FrameInfo *> get_frames( unsigned frame_no, const DecoderHash & decoder ) const
  {
    // FIXME do something smarter here
    vector<const FrameInfo *> intersection;

    for ( const FrameInfo * frame : frames_by_number_[ frame_no ] ) {
      if ( decoder.can_decode( frame->source ) ) {
        intersection.push_back( frame );
      }
    }

    for ( const FrameInfo * frame : frames_by_number_[ frames_by_number_.size() - 1 ] ) {
      assert( not frame->quality.initialized() );
      if ( decoder.can_decode( frame->source ) ) {
        intersection.push_back( frame );
      }
    }

    return intersection;

    //const set<FrameInfo *> by_number = frames_by_number_[ frame_no ];
    //const set<FrameInfo *> unshown = frames_by_number_[ frames_by_number.size() - 1 ];
    //const set<FrameInfo *> no_state = frames_by_state_[ 0 ];
    //const set<FrameInfo *> by_state = frames_by_state_[ decoder.state_hash() ];
    //const set<FrameInfo *> by_last = frames_by_last_[ decoder.last_hash() ];
  }
};

// listS for VertexList to ensure that vertex iterators aren't invalidated
using FrameGraph = boost::adjacency_list<boost::listS, boost::listS, boost::bidirectionalS, VertexState, EdgeState>;
using Vertex = FrameGraph::vertex_descriptor;
using Edge = FrameGraph::edge_descriptor;

// Tracks the resource consumption along a path
// for shortest path with resource constraints
struct PathState
{
  double error;
  long long remaining_buffer;
  bool shown; // Used for determining whether or not to add to remaining_buffer

  PathState( double err, int buf, bool is_shown )
    : error( err ), remaining_buffer( buf ), shown( is_shown )
  {}

  // Optimized for case where we are assigning to ourself
  PathState& operator=( const PathState& other )
  {
    if ( this == &other ) {
      return *this;
    }
    error = other.error;
    remaining_buffer = other.remaining_buffer;
    shown = other.shown;

    return *this;
  }

  bool operator<( const PathState & other ) const
  {
    // We need at least one resource to be strictly increasing
    // error does that ( while remaining_buffer could go up or down )
    if ( error > other.error ) {
      return false;
    }
    if ( error == other.error ) {
      return remaining_buffer > other.remaining_buffer;
    }
    return true;
  }

};

bool path_dominate( const PathState & path_1, const PathState & path_2 )
{
  // This has to be equal to path_1 <= path_2
  return path_1.error <= path_2.error and
    path_1.remaining_buffer >= path_2.remaining_buffer;
}

class PathExtender
{
private:
  unsigned byte_rate_;
  unsigned long long buffer_size_;

public:
  PathExtender( unsigned byte_rate, unsigned buffer_size )
    : byte_rate_( byte_rate ), buffer_size_( buffer_size )
  {}

  bool operator()( const FrameGraph & graph, PathState & new_path,
                   const PathState & old_path, Edge edge ) const
  {
    new_path = old_path;

    // Only have 1/24 of a second if we've displayed another frame
    if ( old_path.shown ) {
      new_path.remaining_buffer += byte_rate_ / 24;
    }

    if ( graph[ edge ].error.initialized() ) { // shown
      new_path.error += graph[ edge ].error.get();
      new_path.shown = true;
    }
    else {
      new_path.shown = false;
    }
    // Error on edge of 0 means error doesn't increase, but remaining_buffer potentially
    // decreases, invalidating old_path <= new_path
    if ( new_path.error == old_path.error ) {
      // Big hack to make error strictly increasing
      new_path.error += 0.0001;
    }
    new_path.remaining_buffer -= graph[ edge ].size;
    return new_path.remaining_buffer >= 0;
  }
};

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
  unsigned cur_edge_;

  // Special terminator vertices, these don't represent actual decoder states
  // but are used for the shortest paths algorithm
  Vertex start_, end_;

  size_t num_vertices( void ) const
  {
    return boost::num_vertices( graph_ );
  }

  // Enforces uniqueness
  Vertex add_vertex( const DecoderHash & cur_decoder, unsigned frame_no, bool last )
  {
    size_t vertex_hash = cur_decoder.hash();
    boost::hash_combine( vertex_hash, frame_no );

    auto v_iter = vertex_map_.find( vertex_hash );
    if ( v_iter != vertex_map_.end() ) {
      // Some hash collision asserts...
      assert( graph_[ v_iter->second ].frame_no == frame_no );
      assert( graph_[ v_iter->second ].cur_decoder == cur_decoder ); 
      return v_iter->second;
    }

    Vertex new_vert = boost::add_vertex( VertexState { cur_decoder, frame_no, num_vertices() }, graph_ );
    vertex_map_.insert( make_pair( vertex_hash, new_vert ) );

    if ( not last ) {
      new_vertices_.push_back( new_vert );
    }

    return new_vert;
  }

  void add_edge( const Vertex & v, const Vertex & u, const FrameInfo * frame )
  {
    Optional<double> quality;
    size_t size = 0;
    if ( frame ) {
      quality = frame->quality;
      size = frame->size;
    }
    boost::add_edge( v, u, EdgeState { frame, quality, size, cur_edge_ }, graph_ );
    cur_edge_ += 1;
  }

#if 0
  // FIXME this might need to check the uniqueness of predecessors
  void cleanup_dead_end( const Vertex & v )
  {
    auto in_edge_iters = boost::in_edges( v, graph_ );

    // Record all the predecessors in the graph
    // FIXME the unordered_set is a hack to work around the multigraph issue
    unordered_set<Vertex> predecessors;
    for ( auto edge_iter = in_edge_iters.first; edge_iter != in_edge_iters.second; edge_iter++ ) {
      Vertex pred = boost::source( *edge_iter, graph_ );
      assert( pred != v ); // Loop
      assert( graph_[ pred ].frame_no <= graph_[ v ].frame_no );

      predecessors.insert( pred );
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
#endif

public:
  GraphManager( const FrameManager & frames )
    : graph_(),
      vertex_map_(),
      new_vertices_(),
      cur_vertex_( 2 ), // Take the start and end vertices into account
      cur_edge_( 0 ),
      start_( boost::add_vertex( VertexState { DecoderHash( 0, 0, 0, 0, 0 ), 0, 0 }, graph_ ) ),
      end_( boost::add_vertex( VertexState { DecoderHash( 0, 0, 0, 0, 0 ), 0, 1 }, graph_ ) )
  {
    for ( const FrameInfo * frame : frames.start_frames() ) {
      Vertex v = add_vertex( DecoderHash( frame->target.state_hash,
                             frame->target.continuation_hash,
                             frame->target.output_hash,
                             frame->target.output_hash,
                             frame->target.output_hash ), 0, false );

      add_edge( start_, v, frame );
    }

    // For every newly created vertices check every frame that it can 
    // potentially decode (ie unshown frames or frames that display the proper next raster)
    while ( not new_vertices_.empty() ) { vector<Vertex> prev_new_vertices( move( new_vertices_ ) ); new_vertices_.clear(); 
      for ( const Vertex v : prev_new_vertices ) {
        const DecoderHash & cur_decoder = graph_[ v ].cur_decoder;
        unsigned cur_frame_no = graph_[ v ].frame_no;
        unsigned next_frame_no = cur_frame_no + 1;

        for ( const FrameInfo * frame : frames.get_frames( next_frame_no, cur_decoder ) )  {
          DecoderHash new_state = cur_decoder;
          new_state.update( frame->target );

          // If it's shown advance the frame number
          unsigned new_frame_no = frame->target.shown ? next_frame_no : cur_frame_no;

          // end of the road, so don't put this in new_vertices_
          bool last = new_frame_no == frames.num_frames();

          Vertex new_v = add_vertex( new_state, new_frame_no, last );

          // Connect final vertices to the terminator vertex
          if ( last ) {
            add_edge( new_v, end_, nullptr );
          }

          if ( v != new_v ) {
            // FIXME This edge can already exist which can turn us into a multigraph
            add_edge( v, new_v, frame );
          }
          else {
            // FIXME, this means we have a nop hidden frame which really doesn't make sense? (hash collision?)
            assert( not frame->target.shown ); // If this was a shown frame which produces the same vertex??? (should be impossible as frame_no increments )
          }
        }
      }
    }
    cout << "Graph has " << num_vertices() << " vertices and " <<
      boost::num_edges( graph_ ) << " edges\n";
  }

  // Just don't do it
  GraphManager( const GraphManager & ) = delete;
  GraphManager& operator=( const GraphManager & ) = delete;

  vector<string> get_path( unsigned buffer_size, unsigned byte_rate, unsigned delay_ms ) const
  {
    auto vertex_id = boost::get( &VertexState::id, graph_ );
    auto edge_id = boost::get( &VertexState::id, graph_ );

    unsigned start_buffer = byte_rate * delay_ms / 1000;

    vector<Edge> solution;
    vector<string> frames;

    PathState solution_state( 0, 0, 0 );

    boost::r_c_shortest_paths( graph_, vertex_id, edge_id, start_, end_, solution,
        solution_state, PathState( 0, start_buffer, false ), PathExtender( byte_rate, buffer_size ), path_dominate );

    if ( solution.size() == 0 ) {
      cout << "No path found within resource constraints\n";
      return frames;
    }

    frames.reserve( solution.size() - 1 );
    // Boost stores the path backwards. Don't want the last component which is a fake frame to a
    // terminating vertex
    for ( unsigned i = solution.size() - 1 ; i > 0; i-- ) {
      frames.push_back( graph_[ solution[ i ] ].frame->name );
    }

    return frames;
  }

  void dump_graph( const string & graph_name ) const 
  {
    ofstream dot_file( graph_name );

    auto vertex_name = boost::get( &VertexState::cur_decoder, graph_ );
    auto edge_name = boost::get( &EdgeState::id, graph_ );

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

  // These factors determine if a path ends of being viable or not
  long max_buffer_size = stol( argv[ 2 ] );
  unsigned long byte_rate = stoul( argv [ 3 ] ) / 8;
  unsigned long delay = stoul( argv[ 4 ] );

  ifstream original_manifest( "original_manifest" );
  ifstream quality_manifest( "quality_manifest" );
  ifstream frame_manifest( "frame_manifest" );

  uint16_t width, height;
  original_manifest >> width >> height;

  FrameManager frames( original_manifest, quality_manifest, frame_manifest );

  clock_t start = clock();
  GraphManager graph( frames );
  //graph.dump_graph( "graph_out.dot" );
  clock_t done = clock();
  cout << "Generating graph took " << ( done - start )*1000 / CLOCKS_PER_SEC << " milliseconds\n";
  start = clock();
  vector<string> path = graph.get_path( max_buffer_size, byte_rate, delay );
  done = clock();
  cout << "Finding path took " << ( done - start )*1000 / CLOCKS_PER_SEC << " milliseconds\n";

  cout << "Video is " << frames.num_frames() << " frames long\n";
  cout << "Path is " << path.size() << " frames long\n";

  FramePlayer player { width, height };
  VideoDisplay display { player.example_raster() };

  for ( string frame_name : path ) {
    SerializedFrame frame { frame_name };
    cout << frame_name << "\n";

    Optional<RasterHandle> output = player.decode( frame );
    if ( output.initialized() ) {
      display.draw( output.get() );
    }
  }
}
