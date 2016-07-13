/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include "player.hh"
#include "display.hh"

#include <fstream>
#include <sstream>

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/r_c_shortest_paths.hpp>
#include <ctime>

using namespace std;

struct EdgeState
{
  double error; // Error from shown frame, cached for use by rc_shortest
  size_t size; // cumulative size of frames, cached for use as edge weight
  unsigned id;
};

using FrameGraph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, boost::no_property, EdgeState>;
using Vertex = FrameGraph::vertex_descriptor;
using Edge = FrameGraph::edge_descriptor;
using VertexIterator = FrameGraph::vertex_iterator;

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
  FrameGraph graph_;

  // Special terminator vertices, these don't represent actual decoder states
  // but are used for the shortest paths algorithm
  Vertex start_, end_;

  // FIXME This is a bit of a hack, but the width and height are currently just
  // serialized into the same file as everything else (
  unsigned width_, height_;

public:
  GraphManager( const string & serialized_filename )
    : graph_(),
      start_(),
      end_(),
      width_(),
      height_()
  {
    ifstream graph_file( serialized_filename );

    graph_file >> width_ >> height_;
    unsigned num_vertices, num_edges;

    // Setup vertices
    graph_file >> num_vertices;
    graph_ = FrameGraph( num_vertices );
    VertexIterator begin_vert, end_vert;
    tie( begin_vert, end_vert ) = boost::vertices( graph_ );
    start_ = *begin_vert;
    end_ = *( begin_vert + 1 );

    // Setup edges
    graph_file >> num_edges;
    for ( unsigned id = 0; id < num_edges; id++ ) {
      unsigned source_id, target_id;
      double error;
      size_t size;
      graph_file >> source_id >> target_id >> error >> size;

      boost::add_edge( source_id, target_id, EdgeState { error, size, id }, graph_ );
    }
  }

  // Just don't do it
  GraphManager( const GraphManager & ) = delete;
  GraphManager& operator=( const GraphManager & ) = delete;

  vector<unsigned> get_path( unsigned buffer_size, unsigned byte_rate, unsigned delay_ms ) const
  {
    auto vertex_id = boost::get( boost::vertex_index, graph_ );
    auto edge_id = boost::get( &EdgeState::id, graph_ );

    unsigned start_buffer = byte_rate * delay_ms / 1000;

    vector<vector<Edge>> solutions;
    vector<PathState> solution_states;

    boost::r_c_shortest_paths( graph_, vertex_id, edge_id, start_, end_, solutions,
        solution_states, PathState( 0, start_buffer ), PathExtender( byte_rate, buffer_size ), path_dominate );

    vector<unsigned> solution_edges;
    if ( solutions.size() == 0 ) {
      cout << "No path found within resource constraints\n";
      // FIXME call djikstra to get absolute minimum size path (or precalc this?)
      return solution_edges;
    }

    cout << solution_states.size() << " undominated paths\n";

    // solutions contains all the undominated paths, since a path only dominates another
    // path if it's error is less than or equal *and* it has more ( or equal ) buffer,
    // solutions will contain a lot of paths with different errors, so at this point pick
    // the one with the lowest error since we don't care about remaining buffer at the end
    // of the video
    double minerror = INFINITY;
    unsigned best_path_idx = 0;
    for ( unsigned i = 0; i < solution_states.size(); i++ ) {
      const PathState & path = solution_states[ i ];
      if ( path.error < minerror ) {
        best_path_idx = i;
      }
    }
    const vector<Edge> & solution = solutions[ best_path_idx ];

    solution_edges.reserve( solution.size() );
    // Boost stores the path backwards. Don't want the last edge which doesn't have a frame
    // ( since it is going to the fake terminating vertex )
    for ( auto edge_iter = solution.rbegin(); edge_iter != solution.rend() - 1; edge_iter++ ) {
      solution_edges.push_back( graph_[ *edge_iter ].id );
    }

    return solution_edges;
  }

  uint16_t get_video_width( void )
  {
    return width_;
  }

  uint16_t get_video_height( void )
  {
    return height_;
  }
};

static vector<vector<string>> process_edge_map( ifstream & edge_file )
{
  vector<vector<string>> edge_map;

  while ( not edge_file.eof() ) {
    string edge_line;
    getline( edge_file, edge_line );
    istringstream line_stream( edge_line );

    vector<string> edge_frames { istream_iterator<string>{ line_stream }, istream_iterator<string>{} };

    edge_map.emplace_back( move( edge_frames ) );
  }

  return edge_map;
}

int main( int argc, char * argv[] )
{
  if ( argc < 7 ) {
    cerr << "Usage: GRAPH_FILE EDGE_MAP VIDEO_DIR BUFFER_SIZE BIT_RATE DELAY\n";
    return EXIT_FAILURE;
  }

  // These factors determine if a path ends of being viable or not
  long max_buffer_size = stol( argv[ 4 ] );
  unsigned long byte_rate = stoul( argv [ 5 ] ) / 8;
  unsigned long delay = stoul( argv[ 6 ] );

  GraphManager graph( argv[ 1 ] );
  ifstream edge_map_file( argv[ 2 ] );

  // In a real network player, the edge_map would be stored on the server side
  // and the client would send the edge list to the server and the server
  // would send back the frames
  vector<vector<string>> edge_map = process_edge_map( edge_map_file );

  clock_t start = clock();
  vector<unsigned> path = graph.get_path( max_buffer_size, byte_rate, delay );
  clock_t done = clock();

  cout << "Finding path took " << ( done - start ) * 1000 / CLOCKS_PER_SEC << " milliseconds\n";
  cout << "Path is " << path.size() << " frames long\n";

  // chdir into the video directory so we can open the frames to play them back
  if ( chdir( argv[ 3 ] ) ) {
    throw unix_error( "Couldn't chdir into VIDEO_DIR" );
  }

  FramePlayer player { graph.get_video_width(), graph.get_video_height() };
  VideoDisplay display { player.example_raster() };

  for ( unsigned edge_id : path ) {
    for ( const string & frame_name : edge_map[ edge_id ] ) {
      SerializedFrame frame { frame_name };
      cout << frame_name << "\n";

      Optional<RasterHandle> output = player.decode( frame );
      if ( output.initialized() ) {
        display.draw( output.get() );
      }
    }
  }
}
