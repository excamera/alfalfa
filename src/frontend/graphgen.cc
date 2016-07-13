/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include "dependency_tracking.hh"
#include "exception.hh"

#include <fstream>
#include <iterator>
#include <climits>
#include <cstdlib>
#include <vector>
#include <unordered_map>

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>

using namespace std;

struct FrameInfo
{
  SourceHash source;
  TargetHash target;
  Optional<double> quality;
  unsigned int size;
  string name;

  vector<const FrameInfo *> guaranteed_nexts;
};

class FrameManager
{
private:
  // This stores all the actual FrameInfo's, everything else points to it
  list<FrameInfo> all_frames_;

  vector<vector<FrameInfo *>> frames_by_number_;
  vector<FrameInfo *> unshown_frames_;

public:
  FrameManager( ifstream & original_manifest, ifstream & quality_manifest, ifstream & frame_manifest )
    : all_frames_(), frames_by_number_(), unshown_frames_()
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

      double quality = stod( quality_string );
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
    vector<const FrameInfo *> unambiguous_deps;

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
        FrameInfo frame { source, target, Optional<double>(), size, frame_name, {} };
        all_frames_.push_back( frame );
        if ( not source.last_hash.initialized() and not source.golden_hash.initialized() and
             not source.alt_hash.initialized() and source.state_hash.initialized() and 
             source.continuation_hash.initialized() ) {
          unambiguous_deps.push_back( &all_frames_.back() );
        }
        else {
          unshown_frames_.push_back( &all_frames_.back() );
        }
      }
      else {
        // For each possible output
        for ( const auto & output_info : output_quality_map.find( target.output_hash )->second ) {
          FrameInfo frame { source, target, make_optional( true, output_info.first ), size, frame_name, {} };
          all_frames_.push_back( frame );
          frames_by_number_[ output_info.second ].push_back( &all_frames_.back() );
        }
      }
    }

    // Huge amount of time is spent figuring out which unshown frames can be decoded, so precalculate it here
    for ( const vector<FrameInfo *> & cur_frames : frames_by_number_ ) {
      for ( FrameInfo * frame : cur_frames ) {
        size_t cur_state = frame->target.state_hash;
        size_t cur_continuation = frame->target.continuation_hash;

        for ( const FrameInfo * unshown_frame : unambiguous_deps ) {
          if ( cur_state == unshown_frame->source.state_hash.get() and
               cur_continuation == unshown_frame->source.continuation_hash.get() ) {
            frame->guaranteed_nexts.push_back( unshown_frame );
          }
        }
      }
    }

    for ( FrameInfo * frame : unshown_frames_ ) {
      size_t cur_state = frame->target.state_hash;
      size_t cur_continuation = frame->target.continuation_hash;

      for ( const FrameInfo * unshown_frame : unambiguous_deps ) {
        if ( cur_state == unshown_frame->source.state_hash.get() and
             cur_continuation == unshown_frame->source.continuation_hash.get() ) {
          frame->guaranteed_nexts.push_back( unshown_frame );
        }
      }
    }
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
    vector<const FrameInfo *> possibles;

    for ( const FrameInfo * frame : frames_by_number_[ frame_no ] ) {
      possibles.push_back( frame );
    }

    for ( const FrameInfo * frame : unshown_frames_ ) {
      assert( not frame->quality.initialized() );
      possibles.push_back( frame );
    }

    return possibles;
  }
};

struct VertexState
{
  DecoderHash cur_decoder;
  const FrameInfo * last_frame;
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

  EdgeState( const vector<const FrameInfo *> & edge_frames )
    : frames( move( edge_frames ) ), error( 0 ), size( 0 )
  {
    for ( const FrameInfo * frame : frames ) {
      size += frame->size;
    }
    // Round up size to nearest 10, this makes path equivalence more likely
    size = ((size + 10 - 1) / 10) * 10;

    // FIXME get rid of the special case for the fake last edge with no frames
    if ( frames.size() > 0 and frames.back()->target.shown ) {
      // for SSIM 1 is the highest quality and 0 is the lowest, so error is 1 - SSIM
      // so larger error is worse quality
      error = 1 - frames.back()->quality.get();
    }
    if ( error == 0 ) {
      // FIXME error has to be strictly increasing so we give it a small error
      // even though it is technically perfect. Should come up with a real lower
      // bound for this
      error = 0.000001;
    }
  }
};

// listS for VertexList to ensure that vertex iterators aren't invalidated
using FrameGraph = boost::adjacency_list<boost::listS, boost::listS, boost::bidirectionalS, VertexState, EdgeState>;
using Vertex = FrameGraph::vertex_descriptor;
using VertexIterator = FrameGraph::vertex_iterator;
using Edge = FrameGraph::edge_descriptor;
using EdgeIterator = FrameGraph::edge_iterator;
using OutEdgeIterator = FrameGraph::out_edge_iterator;
using InEdgeIterator = FrameGraph::in_edge_iterator;

class GraphGenerator
{
private:
  FrameGraph graph_;

  vector<Vertex> shown_vertices_; // Tracks the shown vertices added in a given step
  // Hash of DecoderHash -> actual vertex
  unordered_map<size_t, Vertex> shown_vertex_hashes_; // This stops us from introducing duplicate vertices

  vector<Vertex> unshown_vertices_;
  // Separate hash for unshown vertices to avoid strange cases where unshown frames lead to the same state
  // as a shown frame
  unordered_map<size_t, Vertex> unshown_vertex_hashes_;

  // Special terminator vertices, these don't represent actual decoder states
  // but are used for the shortest paths algorithm
  Vertex start_, end_;

  // Enforces uniqueness
  Vertex add_vertex( const DecoderHash & cur_decoder, const FrameInfo * frame,
                     vector<Vertex> & vertices, unordered_map<size_t, Vertex> & vertex_hashes )
  {
    size_t vertex_hash = cur_decoder.hash();

    auto v_iter = vertex_hashes.find( vertex_hash );
    if ( v_iter != vertex_hashes.end() ) {
      // Some hash collision asserts...
      assert( graph_[ v_iter->second ].cur_decoder == cur_decoder ); 
      return v_iter->second;
    }

    // Give every vertex an id of 0. After graph pruning is done we will iterate
    // through and assign each vertex a real id (otherwise there are gaps in the
    // range after pruning)
    Vertex new_vert = boost::add_vertex( VertexState { cur_decoder, frame }, graph_ );
    vertex_hashes.insert( make_pair( vertex_hash, new_vert ) );
    vertices.push_back( new_vert );

    return new_vert;
  }

  Vertex add_unshown_vertex( const DecoderHash & cur_decoder, const FrameInfo * frame )
  {
    return add_vertex( cur_decoder, frame, unshown_vertices_, unshown_vertex_hashes_ );
  }

  Vertex add_shown_vertex( const DecoderHash & cur_decoder, const FrameInfo * frame )
  {
    return add_vertex( cur_decoder, frame, shown_vertices_, shown_vertex_hashes_ );
  }

  void add_edge( const Vertex & v, const Vertex & u, const vector<const FrameInfo *> & frames )
  {
    Edge old_edge;
    bool duplicate;
    tie( old_edge, duplicate ) = boost::edge( v, u, graph_ );

    // Same as vertices, id's will be assigned after pruning
    EdgeState new_edge_state( frames );

    // If an edge already exists here, pick the smallest one
    if ( duplicate ) {
      unsigned old_size = graph_[ old_edge ].size;

      assert( new_edge_state.error == graph_[ old_edge ].error );
      if ( new_edge_state.size < old_size ) {
        graph_[ old_edge ] = new_edge_state;
      }
    }
    else {
      boost::add_edge( v, u, new_edge_state, graph_ );
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

  void make_new_vertex( const Vertex & old_vertex, const DecoderHash & old_state,
                        const FrameInfo * frame )
  {
    DecoderHash new_state( old_state );
    new_state.update( frame->target );

    if ( frame->target.shown ) {
      Vertex new_shown = add_shown_vertex( new_state, frame );
      // Since this is a shown vertex this should never be the case
      assert( old_vertex != new_shown );
      add_edge( old_vertex, new_shown, { frame } );
    }
    else {
      Vertex new_unshown = add_unshown_vertex( new_state, frame );

      // There seem to be nop unshown frames... (these would make a loop)
      if ( old_vertex != new_unshown ) {
        add_edge( old_vertex, new_unshown, { frame } );
      }
    }
  }

  void make_new_vertices( const vector<Vertex> & vertices, const vector<const FrameInfo *> & possible_frames )
  {
    for ( Vertex cur_vertex : vertices ) {
      const DecoderHash & cur_decoder = graph_[ cur_vertex ].cur_decoder;
      for ( const FrameInfo * frame : possible_frames ) {
        if ( cur_decoder.can_decode( frame->source ) ) {
          make_new_vertex( cur_vertex, cur_decoder, frame );
        }
      }
      for ( const FrameInfo * frame : graph_[ cur_vertex ].last_frame->guaranteed_nexts ) {
        assert( cur_decoder.can_decode( frame->source ) );
        make_new_vertex( cur_vertex, cur_decoder, frame );
      }
    }
  }

  vector<Vertex> extend_unshowns( const vector<const FrameInfo *> & possible_frames )
  {
    vector<Vertex> all_unshown;

    while ( not unshown_vertices_.empty() ) {
      vector<Vertex> prev_unshown( move( unshown_vertices_ ) );
      unshown_vertices_.clear();

      all_unshown.insert( all_unshown.end(), prev_unshown.begin(), prev_unshown.end() );

      make_new_vertices( prev_unshown, possible_frames );
    }

    return all_unshown;
  }

  void merge_unshown_edges( const vector<Vertex> & unshown_vertices )
  {
    for ( Vertex unshown_vert : unshown_vertices ) {
      OutEdgeIterator outgoing_start, outgoing_end;
      InEdgeIterator incoming_start, incoming_end;
      tie( outgoing_start, outgoing_end ) = boost::out_edges( unshown_vert, graph_ );
      tie( incoming_start, incoming_end ) = boost::in_edges( unshown_vert, graph_ );
      for ( OutEdgeIterator outgoing_iter = outgoing_start; outgoing_iter != outgoing_end; outgoing_iter++ ) {
        Vertex target = boost::target( *outgoing_iter, graph_ );
        const vector<const FrameInfo *> & target_frames( graph_[ *outgoing_iter ].frames );
        for ( InEdgeIterator incoming_iter = incoming_start; incoming_iter != incoming_end; incoming_iter++ ) {
          Vertex source = boost::source( *incoming_iter, graph_ );

          vector<const FrameInfo *> combined_frames( graph_[ *incoming_iter ].frames );

          combined_frames.insert( combined_frames.end(), target_frames.begin(), target_frames.end() );

          add_edge( source, target, combined_frames );
        }
      }

      boost::clear_vertex( unshown_vert, graph_ );
      boost::remove_vertex( unshown_vert, graph_ );
    }
  }

public:
  GraphGenerator( const FrameManager & frames )
    : graph_(),
      shown_vertices_(),
      shown_vertex_hashes_(),
      unshown_vertices_(),
      unshown_vertex_hashes_(),
      start_( boost::add_vertex( VertexState { DecoderHash( 0, 0, 0, 0, 0 ), nullptr }, graph_ ) ),
      end_( boost::add_vertex( VertexState { DecoderHash( 0, 0, 0, 0, 0 ), nullptr }, graph_ ) )
  {
    for ( const FrameInfo * frame : frames.start_frames() ) {
      Vertex v = add_shown_vertex( DecoderHash( frame->target.state_hash,
                                                frame->target.continuation_hash,
                                                frame->target.output_hash,
                                                frame->target.output_hash,
                                                frame->target.output_hash ), frame );
      add_edge( start_, v, { frame } );
    }

    // For every frame in the video generate all the possible vertices for it
    for ( unsigned int cur_frame_num = 1; cur_frame_num < frames.num_frames(); cur_frame_num++ ) {
      assert( not shown_vertices_.empty() ); // No valid way to play the video??

      vector<Vertex> prev_frame_vertices( move( shown_vertices_ ) );
      shown_vertices_.clear();
      shown_vertex_hashes_.clear();
      unshown_vertices_.clear();
      unshown_vertex_hashes_.clear();

      vector<const FrameInfo *> possible_frames = frames.get_frames( cur_frame_num );

      // Go through all the vertices from the previous frames
      make_new_vertices( prev_frame_vertices, possible_frames );

      vector<Vertex> unshowns = extend_unshowns( possible_frames );

      // Merge unshown frame edges
      merge_unshown_edges( unshowns );
    }

    // shown_vertices_ holds all the vertices that were reached with the last frame
    // Loop through these and connect them to the special terminator vertex
    for ( Vertex last : shown_vertices_ ) {
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

    cout << "Graph has " << boost::num_vertices( graph_ ) << " vertices and " <<
      boost::num_edges( graph_ ) << " edges\n";
  }

  // This class would need a custom copy constructor under Weffc++ because of
  // start_ and end_ (which are really void*'s), but there is no point, so
  // just disable copying
  GraphGenerator( const GraphGenerator & ) = delete;
  GraphGenerator& operator=( const GraphGenerator & ) = delete;

  void serialize( ofstream & output_file, ofstream & edge_map, unsigned width, unsigned height ) const
  {
    output_file << width << " " << height << endl;

    output_file << boost::num_vertices( graph_ ) << endl;
    output_file << boost::num_edges( graph_ ) << endl;

    // Assign ids to vertices
    unordered_map<Vertex, unsigned> vertex_ids;

    VertexIterator vertex_iter, vertex_iter_end;
    tie( vertex_iter, vertex_iter_end ) = boost::vertices( graph_ );

    // Recreation of the graph relies on start_ being id 0 and end_ being id 1
    assert( *vertex_iter == start_ and *( next( vertex_iter ) ) == end_ );

    // Assign each vertex an id / index
    unsigned id = 0;
    for ( ; vertex_iter != vertex_iter_end; vertex_iter++ ) {
      vertex_ids[ *vertex_iter ] = id;
      id++;
    }

    // Write out edges in format: source_vertex target_vertex error size
    EdgeIterator edge_iter, edge_iter_end;
    tie( edge_iter, edge_iter_end ) = boost::edges( graph_ );
    for ( ; edge_iter != edge_iter_end; edge_iter++ ) {
      Edge e = *edge_iter;
      double error = graph_[ e ].error;
      size_t size = graph_[ e ].size;
      Vertex source = boost::source( e, graph_ );
      Vertex target = boost::target( e, graph_ );

      output_file << vertex_ids[ source ] << " " << vertex_ids[ target ] <<
        " " << error << " " << size << endl;

      for ( const FrameInfo * frame : graph_[ e ].frames ) {
        edge_map << frame->name << " ";
      }
      edge_map << endl;
    }
  }
};

int main( int argc, char * argv[] )
{
  if ( argc < 4 ) {
    cerr << "Usage: VIDEO_DIR GRAPH_OUTPUT EDGE_MAP\n";
    return EXIT_FAILURE;
  }

  ofstream graph_output( argv[ 2 ] );
  ofstream edge_map( argv[ 3 ] );

  if ( chdir( argv[ 1 ] ) ) {
    throw unix_error( "Couldn't chdir into VIDEO_DIR" );
  }

  ifstream original_manifest( "original_manifest" );
  ifstream quality_manifest( "quality_manifest" );
  ifstream frame_manifest( "frame_manifest" );

  unsigned width, height;
  original_manifest >> width >> height;

  FrameManager frame_manager( original_manifest, quality_manifest, frame_manifest );

  GraphGenerator graph( frame_manager );

  graph.serialize( graph_output, edge_map, width, height );

  return EXIT_SUCCESS;
}
