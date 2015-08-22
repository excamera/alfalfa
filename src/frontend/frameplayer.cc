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

class FrameManager
{
private:
  // This stores all the actual FrameInfo's, everything else points to it
  list<FrameInfo> all_frames_;

  vector<vector<const FrameInfo *>> frames_by_number_;
  vector<const FrameInfo *> unshown_frames_;

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
    : all_frames_(), frames_by_number_(), unshown_frames_(), frames_by_state_(), frames_by_continuation_(),
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
    frames_by_number_.resize( frame_no );

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
        unshown_frames_.push_back( &all_frames_.back() );
      }
      else {
        // For each possible output
        for ( const auto & output_info : output_quality_map.find( target.output_hash )->second ) {
          FrameInfo frame { source, target, make_optional( true, output_info.first ), size, frame_name };
          all_frames_.push_back( frame );
          frames_by_number_[ output_info.second ].push_back( &all_frames_.back() );
        }
      }
    }
    index_frames();
  }

  unsigned num_frames( void ) const
  {
    return frames_by_number_.size();
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

  vector<const FrameInfo *> get_frames( unsigned frame_no ) const
  {
    vector<const FrameInfo *> intersection;

    for ( const FrameInfo * frame : frames_by_number_[ frame_no ] ) {
      intersection.push_back( frame );
    }

    for ( const FrameInfo * frame : unshown_frames_ ) {
      assert( not frame->quality.initialized() );
      intersection.push_back( frame );
    }

    return intersection;

    //const set<FrameInfo *> by_number = frames_by_number_[ frame_no ];
    //const set<FrameInfo *> unshown = frames_by_number_[ frames_by_number.size() - 1 ];
    //const set<FrameInfo *> no_state = frames_by_state_[ 0 ];
    //const set<FrameInfo *> by_state = frames_by_state_[ decoder.state_hash() ];
    //const set<FrameInfo *> by_last = frames_by_last_[ decoder.last_hash() ];
  }

  vector<const FrameInfo *> filter_frames( const vector<const FrameInfo *> & frames, const DecoderHash & cur_decoder ) const
  {
    // FIXME do something smarter here
    vector<const FrameInfo *> intersection;
    for ( const FrameInfo * frame : frames ) {
      if ( cur_decoder.can_decode( frame->source ) ) {
        intersection.push_back( frame );
      }
    }

    return intersection;
  }
};

struct VertexState
{
  DecoderHash cur_decoder;
  size_t id;
};

// One edge can represent multiple frames, at most one frame can be
// shown, and it is always the last entry in frames. This considerably reduces the
// complexity of rc_shortest_paths, since error is now strictly increasing 
// ( with error of 0 rounded up to a low number ), and the algorithm will no longer
// make incorrect choices by preferring a path with fewer edges
struct EdgeState
{
  vector<const FrameInfo *> frames;
  double error; // Error from shown frame, cached for use by rc_shortest
  size_t size; // cumulative size of frames, cached for use as edge weight
  size_t id;

  EdgeState( const vector<const FrameInfo *> & edge_frames, unsigned new_id )
    : frames( move( edge_frames ) ), error( 0 ), size( 0 ), id( new_id )
  {
    for ( const FrameInfo * frame : frames ) {
      size += frame->size;
    }
    if ( frames.size() > 0 ) {
      error = frames.back()->quality.get();
    }
    if ( error == 0 ) {
      // FIXME error has to be strictly increasing so we give it a small error
      // even though it is technically perfect. Should come up with a real lower
      // bound for this
      error = 0.000001;
    }
  }
};

// List of unshown frames plus the decoder representing the current state.
// Once it decodes a shown frame it will be promoted to an EdgeState
// and an edge will be created
struct PartialEdge
{
  vector<const FrameInfo *> frames;
  vector<DecoderHash> decoders; // Need a list of previous decoders to avoid loops
};

// listS for VertexList to ensure that vertex iterators aren't invalidated
using FrameGraph = boost::adjacency_list<boost::listS, boost::listS, boost::bidirectionalS, VertexState, EdgeState>;
using Vertex = FrameGraph::vertex_descriptor;
using VertexIterator = FrameGraph::vertex_iterator;
using Edge = FrameGraph::edge_descriptor;
using EdgeIterator = FrameGraph::edge_iterator;

// Tracks the resource consumption along a path
// for shortest path with resource constraints
struct PathState
{
  double error;
  long long remaining_buffer;
  unsigned frame_num;

  PathState( double err, int buf )
    : error( err ), remaining_buffer( buf ), frame_num( 0 )
  {}

  // Optimized for case where we are assigning to ourself
  PathState& operator=( const PathState& other )
  {
    if ( this == &other ) {
      return *this;
    }
    error = other.error;
    remaining_buffer = other.remaining_buffer;

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
  long long buffer_size_;

public:
  PathExtender( unsigned byte_rate, unsigned buffer_size )
    : byte_rate_( byte_rate ), buffer_size_( buffer_size )
  {}

  bool operator()( const FrameGraph & graph, PathState & new_path,
                   const PathState & old_path, Edge edge ) const
  {
    new_path = old_path;

    // Each edge is a shown frame so we gain 1/24 of a second of data
    long long new_remain = old_path.remaining_buffer + byte_rate_ / 24;

    // Stop remaining_buffer from growing above our actual buffer size
    new_path.remaining_buffer = min( new_remain, buffer_size_ );

    new_path.remaining_buffer -= graph[ edge ].size;

    new_path.error += graph[ edge ].error;

    // Error needs to be strictly increasing, and graph gen should take care of that
    assert( new_path.error != old_path.error );

    // Invariant for boost
    assert( old_path < new_path or ( old_path < new_path and new_path < old_path ) );

    return new_path.remaining_buffer >= 0;
  }
};

class GraphManager
{
private:
  FrameGraph graph_;

  vector<Vertex> new_vertices_;
  // Hash of DecoderHash -> actual vertex
  unordered_map<size_t, Vertex> new_vertex_hashes_; // This stops us from introducing duplicate vertices

  // Special terminator vertices, these don't represent actual decoder states
  // but are used for the shortest paths algorithm
  Vertex start_, end_;

  // Enforces uniqueness
  Vertex add_vertex( const DecoderHash & cur_decoder )
  {
    size_t vertex_hash = cur_decoder.hash();

    auto v_iter = new_vertex_hashes_.find( vertex_hash );
    if ( v_iter != new_vertex_hashes_.end() ) {
      // Some hash collision asserts...
      assert( graph_[ v_iter->second ].cur_decoder == cur_decoder ); 
      return v_iter->second;
    }

    // Give every vertex an id of 0. After graph pruning is done we will iterate
    // through and assign each vertex a real id (otherwise there are gaps in the
    // range after pruning)
    Vertex new_vert = boost::add_vertex( VertexState { cur_decoder, 0 }, graph_ );
    new_vertex_hashes_.insert( make_pair( vertex_hash, new_vert ) );
    new_vertices_.push_back( new_vert );

    return new_vert;
  }

  void add_edge( const Vertex & v, const Vertex & u, const vector<const FrameInfo *> & frames )
  {
    Edge old_edge;
    bool duplicate;
    tie( old_edge, duplicate ) = boost::edge( v, u, graph_ );

    // If an edge already exists here, pick the smallest one
    if ( duplicate ) {
      unsigned old_size = graph_[ old_edge ].size;
      EdgeState new_edge( frames, 0 );

      assert( new_edge.error == graph_[ old_edge ].error );
      if ( new_edge.size < old_size ) {
        graph_[ old_edge ] = new_edge;
      }
    }
    else {
      // Same as vertices, id's will be assigned after pruning
      EdgeState new_edge( frames, 0 );

      boost::add_edge( v, u, new_edge, graph_ );
    }
  }

  void cleanup_dead_end( const Vertex & v )
  {
    auto in_edge_iters = boost::in_edges( v, graph_ );

    // Record all the predecessors in the graph
    vector<Vertex> predecessors;
    for ( auto edge_iter = in_edge_iters.first; edge_iter != in_edge_iters.second; edge_iter++ ) {
      Vertex pred = boost::source( *edge_iter, graph_ );
      assert( pred != v ); // Loop

      predecessors.push_back( pred );
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

  vector<PartialEdge> extend_unshown( const FrameInfo * start_frame, const FrameManager & frame_manager,
                                      const vector<const FrameInfo *> & possible_frames,
                                      const DecoderHash & start_decoder )
  {
    vector<PartialEdge> finished_partials;
    vector<PartialEdge> unfinished_partials;
    unfinished_partials.emplace_back( PartialEdge { { start_frame }, { start_decoder } } );

    // We keep looping through the partials until they decode a shown frame,
    // similar logic to how vertices work
    while ( not unfinished_partials.empty() ) {
      vector<PartialEdge> prev_partials( move( unfinished_partials ) );
      unfinished_partials.clear();

      for ( const PartialEdge & unfinished : prev_partials ) {
        for ( const FrameInfo * frame : frame_manager.filter_frames( possible_frames, unfinished.decoders.back() ) ) {
          PartialEdge new_partial( unfinished );

          DecoderHash cur_decoder = new_partial.decoders.back();
          cur_decoder.update( frame->target );

          bool loop = false;
          for ( const DecoderHash & prev_decoder : new_partial.decoders ) {
            if ( cur_decoder == prev_decoder ) {
              loop = true;
              break;
            }
          }
          // If we produced a decoder that we've already produced it's a loop
          // so don't make a new partial from this frame
          if ( loop ) {
            continue;
          }
          new_partial.decoders.push_back( cur_decoder );
          new_partial.frames.push_back( frame );

          if ( frame->target.shown ) {
            finished_partials.emplace_back( new_partial );
          } else {
            unfinished_partials.emplace_back( new_partial );
          }
        }
      }
    }

    return finished_partials;
  }

public:
  GraphManager( const FrameManager & frames )
    : graph_(),
      new_vertices_(),
      new_vertex_hashes_(),
      start_( boost::add_vertex( VertexState { DecoderHash( 0, 0, 0, 0, 0 ), 0 }, graph_ ) ),
      end_( boost::add_vertex( VertexState { DecoderHash( 0, 0, 0, 0, 0 ), 0 }, graph_ ) )
  {
    for ( const FrameInfo * frame : frames.start_frames() ) {
      Vertex v = add_vertex( DecoderHash( frame->target.state_hash,
                                          frame->target.continuation_hash,
                                          frame->target.output_hash,
                                          frame->target.output_hash,
                                          frame->target.output_hash ) );
      add_edge( start_, v, { frame } );
    }

    // For every frame in the video generate all the possible vertices for it

    for ( unsigned int cur_frame_num = 1; cur_frame_num < frames.num_frames(); cur_frame_num++ ) {
      assert( not new_vertices_.empty() ); // No valid way to play the video??

      vector<Vertex> prev_frame_vertices( move( new_vertices_ ) );
      new_vertices_.clear();
      new_vertex_hashes_.clear();

      const vector<const FrameInfo *> possible_frames = frames.get_frames( cur_frame_num );

      // Go through all the vertices from the previous frames
      for ( Vertex v : prev_frame_vertices ) {
        const DecoderHash & cur_decoder = graph_[ v ].cur_decoder;

        for ( const FrameInfo * frame : frames.filter_frames( possible_frames, cur_decoder ) )  {
          DecoderHash new_state = cur_decoder;
          new_state.update( frame->target );

          if ( frame->target.shown ) {
            Vertex new_vert = add_vertex( new_state );
            // Since there are no unshown vertices this should never be the case
            assert( v != new_vert );
            add_edge( v, new_vert, { frame } );
          }
          else {
            // Handle unshown frames
            vector<PartialEdge> edges = extend_unshown( frame, frames, possible_frames, new_state );;

            for ( PartialEdge & edge : edges ) {
              Vertex new_vert = add_vertex( edge.decoders.back() );
              assert( v != new_vert );
              add_edge( v, new_vert, edge.frames );
            }
          }
        }
      }
    }

    // new_vertices_ holds all the vertices that were reached with the last frame
    // Loop through these and connect them to the special terminator vertex
    for ( Vertex last : new_vertices_ ) {
      add_edge( last, end_, {} );
    }

    // Cleanup dead ends at the end otherwise we have to update all our tracking containers
    VertexIterator cur_vert, end_vert;
    tie( cur_vert, end_vert ) = boost::vertices( graph_ );
    for ( ; cur_vert != end_vert; cur_vert++ ) {
      Vertex v = *cur_vert;

      // If the vertex isn't the special terminator vertex and has no adjacent vertices, it's a dead end
      if ( boost::out_degree( v, graph_ ) == 0 and v != end_ ) {
        cleanup_dead_end( v );
        // Reset out iterators after the deletion this is inefficient (we iterate over the whole list,
        // when we've already checked a bunch potentially) but it's easy
        tie( cur_vert, end_vert ) = boost::vertices( graph_ );
      }
    }

    // Assign id's
    tie( cur_vert, end_vert ) = boost::vertices( graph_ );
    for ( unsigned id = 0; cur_vert != end_vert; cur_vert++, id++ ) {
      graph_[ *cur_vert ].id = id;
    }

    EdgeIterator cur_edge, end_edge;
    tie( cur_edge, end_edge ) = boost::edges( graph_ );
    for ( unsigned id = 0; cur_edge != end_edge; cur_edge++, id++ ) {
      graph_[ *cur_edge ].id = id;
    }

    cout << "Graph has " << boost::num_vertices( graph_ ) << " vertices and " <<
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
    PathState solution_state( 0, 0 );

    boost::r_c_shortest_paths( graph_, vertex_id, edge_id, start_, end_, solution,
        solution_state, PathState( 0, start_buffer ), PathExtender( byte_rate, buffer_size ), path_dominate );

    vector<string> frames;
    if ( solution.size() == 0 ) {
      cout << "No path found within resource constraints\n";
      // FIXME call djikstra to get absolute minimum size path (or precalc this?)
      return frames;
    }

    frames.reserve( solution.size() * 2 );
    // Boost stores the path backwards. Don't want the last component which is a fake frame to a
    // terminating vertex
    for ( unsigned i = solution.size() - 1 ; i > 0; i-- ) {
      for ( const FrameInfo * frame : graph_[ solution[ i ] ].frames ) {
        frames.push_back( frame->name );
      }
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
