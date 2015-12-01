#include <sysexits.h>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <stack>
#include <chrono>
#include <tuple>
#include <boost/variant.hpp>

#include "alfalfa_video.hh"
#include "player.hh"
#include "display.hh"
#include "2d.hh"

using namespace std;
using namespace std::chrono;

Optional<TrackData> get_next_frame_data( PlayableAlfalfaVideo & video,
                                         size_t frame_id )
{
  auto result = video.get_track_data_by_frame_id( frame_id );

  if ( result.first != result.second ) {
    size_t frame_index = result.first->frame_index;
    size_t end_frame = video.get_track_size( result.first->track_id );

    frame_index++;

    if ( frame_index < end_frame ) {
      return make_optional( true, video.get_frame( result.first->track_id,
        frame_index ) );
    }
  }

  return Optional<TrackData>();
}

vector<pair<size_t, double> > find_target_frames( PlayableAlfalfaVideo & video, size_t raster_index )
{
  size_t target_raster = video.get_raster( raster_index ).hash;
  vector<pair<size_t, double> > target_frames;

  for ( auto its = video.get_quality_data_by_original_raster( target_raster );
        its.first != its.second; its.first++ ) {

    auto frames = video.get_frames_by_output_hash( its.first->approximate_raster );

    for( auto itf = frames.first; itf != frames.second; itf++ ) {
      target_frames.push_back( make_pair( itf->frame_id(), its.first->quality ) );
    }
  }

  return target_frames;
}

bool display_frame( VideoDisplay & display,
                    const Optional<RasterHandle> & raster )
{
  if ( raster.initialized() ) {
    display.draw( raster.get() );
    return true;
  }

  return false;
}

enum DependencyType { STATE, RASTER };

struct DependencyVertex
{
  DependencyType type;
  size_t hash;

  bool operator<( const DependencyVertex & other ) const
  {
    if ( type < other.type )
      return true;
    else if ( type > other.type )
      return false;

    if ( hash < other.hash )
      return true;

    return false;
  }
};

class CacheManager
{
private:
  map<DependencyVertex, boost::variant<DecoderState, RasterHandle> > cache_ = {};

public:
  template<DependencyType DepType, class ObjectType>
  void do_cache( ObjectType obj )
  {
    cache_.insert( make_pair( DependencyVertex{ DepType, obj.hash() }, obj ) );
  }

  void do_cache( const Decoder & decoder )
  {
    do_cache<RASTER>( decoder.get_references().last );
    do_cache<RASTER>( decoder.get_references().golden );
    do_cache<RASTER>( decoder.get_references().alternative_reference );
    do_cache<STATE>( decoder.get_state() );
  }

  template<DependencyType DepType>
  bool has( size_t hash ) const
  {
    return cache_.count( DependencyVertex{ DepType, hash } ) > 0;
  }

  template<DependencyType DepType>
  void remove( size_t hash )
  {
    DependencyVertex vertex{ DepType, hash };
    cache_.erase( vertex );
  }

  template<DependencyType DepType, class ObjectType>
  ObjectType get( size_t hash )
  {
    DependencyVertex vertex{ DepType, hash };
    return boost::get<ObjectType>( cache_.at( vertex ) );
  }

  void print_cache()
  {
    for ( auto const & item : cache_ )
    {
      if ( item.first.type == RASTER ) {
        cout << "R ";
      }
      else {
        cout << "S ";
      }

      cout << hex << uppercase << item.first.hash << dec << nouppercase << endl;
    }
  }
};

class FrameSeek
{
private:
  const PlayableAlfalfaVideo & video_;
  CacheManager cache_;

  class FrameDependencey
  {
  private:
    map<DependencyVertex, size_t> ref_counter_ = {};
    set<DependencyVertex> unresolved_ = {};

  public:
    template<DependencyType DepType>
    size_t increase_count( const size_t hash )
    {
      DependencyVertex vertex{ DepType, hash };

      if ( ref_counter_.count( vertex ) ) {
        ref_counter_[ vertex ] += 1;
      }
      else {
        ref_counter_[ vertex ] = 1;
      }

      return ref_counter_[ vertex ];
    }

    template<DependencyType DepType>
    size_t decrease_count( const size_t hash )
    {
      DependencyVertex vertex{ DepType, hash };

      if ( ref_counter_.count( vertex ) and ref_counter_[ vertex ]  > 0 ) {
        ref_counter_[ vertex ] -= 1;
        return ref_counter_[ vertex ];
      }

      ref_counter_.erase( vertex );
      return 0;
    }

    template<DependencyType DepType>
    size_t get_count( const size_t hash )
    {
      DependencyVertex vertex{ DepType, hash };

      if ( ref_counter_.count( vertex ) ) {
        return ref_counter_[ vertex ];
      }

      return 0;
    }

    void update_dependencies( const FrameInfo & frame, CacheManager & cache )
    {
      Optional<size_t> hash[] = {
        frame.source_hash().last_hash, frame.source_hash().golden_hash,
        frame.source_hash().alt_hash
      };

      for ( int i = 0; i < 3; i++ ) {
        if ( hash[ i ].initialized() ) {
          if ( cache.has<RASTER>( hash[ i ].get() ) == 0 ) {
            increase_count<RASTER>( hash[ i ].get() );
            unresolved_.insert( DependencyVertex{ RASTER, hash[ i ].get() } );
          }
        }
      }

      if ( frame.source_hash().state_hash.initialized() ) {
        if ( cache.has<STATE>( frame.source_hash().state_hash.get() ) == 0 ) {
          increase_count<STATE>( frame.source_hash().state_hash.get() );
          unresolved_.insert( DependencyVertex{ STATE,
            frame.source_hash().state_hash.get() } );
        }
      }

      unresolved_.erase( DependencyVertex{ RASTER, frame.target_hash().output_hash } );
      unresolved_.erase( DependencyVertex{ STATE, frame.target_hash().state_hash } );
    }

    void update_dependencies_forward( const FrameInfo & frame, CacheManager & cache )
    {
      Optional<size_t> hash[] = {
        frame.source_hash().last_hash, frame.source_hash().golden_hash,
        frame.source_hash().alt_hash
      };

      for ( int i = 0; i < 3; i++ ) {
        if ( hash[ i ].initialized() ) {
          if ( cache.has<RASTER>( hash[ i ].get() ) == 0 ) {
            decrease_count<RASTER>( hash[ i ].get() );
          }

          // if ( get_count<RASTER>( hash[ i ].get() ) == 0 ) {
          //   cache.remove<RASTER>( hash[ i ].get() );
          // }
        }
      }

      if ( frame.source_hash().state_hash.initialized() ) {
        if ( cache.has<STATE>( frame.source_hash().state_hash.get() ) == 0 ) {
          decrease_count<STATE>( frame.source_hash().state_hash.get() );

          // if ( get_count<STATE>( frame.source_hash().state_hash.get() ) == 0 ) {
          //   cache.remove<STATE>( frame.source_hash().state_hash.get() );
          // }
        }
      }
    }

    bool all_resolved()
    {
      return unresolved_.size() == 0;
    }
  };

  struct TrackPath
  {
    size_t track_id;
    size_t start_index;
    size_t end_index;

    friend ostream & operator<<( ostream & os, const TrackPath & path)
    {
      os << "Track " << path.track_id << ": " << path.start_index << " -> "
         << path.end_index;

      return os;
    }
  };

  struct SwitchPath
  {
    size_t from_track_id;
    size_t from_frame_index;
    size_t to_track_id;
    size_t switch_start_index;
    size_t switch_end_index;

    friend ostream & operator<<( ostream & os, const SwitchPath & path)
    {
      os << "Switch: Track " << path.from_track_id << " (" << path.from_frame_index << ") -> Track "
         << path.to_track_id << " [" << path.switch_start_index << ":"
         << path.switch_end_index << "]";

      return os;
    }
  };

  tuple<SwitchPath, Optional<TrackPath>, FrameDependencey, size_t>
  get_min_switch_seek( const size_t output_hash )
  {
    auto frames = video_.get_frames_by_output_hash( output_hash );

    size_t min_cost = SIZE_MAX;
    SwitchPath min_switch_path;
    Optional<TrackPath> min_track_path;
    FrameDependencey min_dependencies;

    for ( auto target_frame = frames.first; target_frame != frames.second; target_frame++ ) {
      auto switches = video_.get_switches_ending_with_frame( target_frame->frame_id() );

      for ( auto sw = switches.begin(); sw != switches.end(); sw++ ) {
        size_t cost = 0;
        FrameDependencey dependencies;

        for ( auto frame = sw->first; frame != sw->second; frame--) {
          cost += frame->length();

          dependencies.update_dependencies( *frame, cache_ );

          if ( dependencies.all_resolved() ) {
            break;
          }
        }

        if ( not dependencies.all_resolved() ) {
          auto track_seek = get_track_seek( sw->first.from_track_id(), sw->first.from_frame_index(),
            dependencies );

          if ( get<2>( track_seek ) == SIZE_MAX ) {
            cost = SIZE_MAX;
            break;
          }

          cost += get<2>( track_seek );

          if ( cost < min_cost ) {
            min_cost = cost;
            min_switch_path.from_track_id = sw->first.from_track_id();
            min_switch_path.to_track_id = sw->first.to_track_id();
            min_switch_path.from_frame_index = sw->first.from_frame_index();
            min_switch_path.switch_start_index = 0;
            min_switch_path.switch_end_index = sw->first.switch_frame_index();

            // min_track_path = TrackPath{ *track_id, get<0>( seek ).frame_index(), frame_index };

            min_track_path.clear();
            min_track_path.initialize( TrackPath{ sw->first.from_track_id(),
              get<0>( track_seek ).frame_index(), sw->first.from_frame_index() } );
            min_dependencies = get<1>( track_seek );
          }
        }
        else {
          if ( cost < min_cost ) {
            min_cost = cost;

            min_switch_path.from_track_id = sw->first.from_track_id();
            min_switch_path.to_track_id = sw->first.to_track_id();
            min_switch_path.from_frame_index = sw->first.from_frame_index();
            min_switch_path.switch_start_index = 0;
            min_switch_path.switch_end_index = sw->first.switch_frame_index();
            min_dependencies = dependencies;
          }
        }
      }
    }

    return make_tuple( min_switch_path, min_track_path, min_dependencies, min_cost );
  }

  tuple<TrackDBIterator, FrameDependencey, size_t>
  get_track_seek( const size_t track_id, const size_t frame_index,
                  FrameDependencey dependencies = {} )
  {
    auto frames_backward = video_.get_frames_reverse( track_id, frame_index );

    if ( frames_backward.first == frames_backward.second ) {
      return make_tuple( frames_backward.second, dependencies, SIZE_MAX );
    }

    size_t cost = 0;

    for( auto frame = frames_backward.first; frame != frames_backward.second; frame-- ) {
      cost += frame->length();

      dependencies.update_dependencies( *frame, cache_ );

      if ( dependencies.all_resolved() ) {
        return make_tuple( frame, dependencies, cost );
      }
    }

    return make_tuple( frames_backward.second, dependencies, SIZE_MAX );
  }

  tuple<TrackPath, FrameDependencey, size_t>
  get_min_track_seek( const size_t frame_index, const size_t output_hash )
  {
    size_t min_cost = SIZE_MAX;
    TrackPath min_track_path;
    FrameDependencey min_frame_dependecy;

    auto track_ids = video_.get_track_ids();

    for ( auto track_id = track_ids.first; track_id != track_ids.second; track_id++ )
    {
      auto track_frame = video_.get_frame( *track_id, frame_index );
      auto frame = video_.get_frame( track_frame.frame_id );

      if ( frame.target_hash().output_hash == output_hash ) {
        tuple<TrackDBIterator, FrameDependencey, size_t> seek = get_track_seek( *track_id, frame_index );

        if ( get<2>( seek ) < min_cost ) {
          min_cost = get<2>( seek );
          min_track_path = TrackPath{ *track_id, get<0>( seek ).frame_index(), frame_index };
          min_frame_dependecy = get<1>( seek );
        }
      }
    }

    return make_tuple( min_track_path, min_frame_dependecy, min_cost );
  }

public:
  FrameSeek( const PlayableAlfalfaVideo & video )
    : video_( video ), cache_()
  {}

  Decoder get_decoder( const FrameInfo & frame )
  {
    References refs( video_.get_info().width, video_.get_info().height );
    DecoderState state( video_.get_info().width, video_.get_info().height );

    if ( frame.source_hash().last_hash.initialized() ) {
      refs.last = cache_.get<RASTER, RasterHandle>( frame.source_hash().last_hash.get() );
    }

    if ( frame.source_hash().golden_hash.initialized() ) {
      refs.golden = cache_.get<RASTER, RasterHandle>( frame.source_hash().golden_hash.get() );
    }

    if ( frame.source_hash().alt_hash.initialized() ) {
      refs.alternative_reference =
        cache_.get<RASTER, RasterHandle>( frame.source_hash().alt_hash.get() );
    }

    if ( frame.source_hash().state_hash.initialized() ) {
      state = cache_.get<STATE, DecoderState>( frame.source_hash().state_hash.get() );
    }

    return Decoder( state, refs );
  }

  FrameDependencey follow_track_path( TrackPath path, FrameDependencey dependencies )
  {
    References refs( video_.get_info().width, video_.get_info().height );
    DecoderState state( video_.get_info().width, video_.get_info().height );

    auto frames = video_.get_frames( path.track_id, path.start_index, path.end_index );

    for ( auto frame = frames.first; frame != frames.second; frame++ ) {
      Decoder && decoder = get_decoder( *frame );
      pair<bool, RasterHandle> output = decoder.get_frame_output( video_.get_chunk( *frame ) );
      cache_.do_cache<RASTER>( output.second );
      cache_.do_cache( decoder );
      dependencies.update_dependencies_forward( *frame, cache_ );
    }

    return dependencies;
  }

  FrameDependencey follow_switch_path( SwitchPath path, FrameDependencey dependencies)
  {
    References refs( video_.get_info().width, video_.get_info().height );
    DecoderState state( video_.get_info().width, video_.get_info().height );

    auto frames = video_.get_frames( path.from_track_id, path.to_track_id,
      path.from_frame_index, path.switch_start_index, path.switch_end_index );

    for ( auto frame = frames.first; frame != frames.second; frame++ ) {
      Decoder && decoder = get_decoder( *frame );
      pair<bool, RasterHandle> output = decoder.get_frame_output( video_.get_chunk( *frame ) );
      cache_.do_cache<RASTER>( output.second );
      cache_.do_cache( decoder );
      dependencies.update_dependencies_forward( *frame, cache_ );
    }

    return dependencies;
  }

  RasterHandle get_raster( const size_t frame_index, const size_t output_hash )
  {
    auto track_seek = get_min_track_seek( frame_index, output_hash );
    auto switch_seek = get_min_switch_seek( output_hash );

    cout << "> Track cost:  " << get<2>( track_seek ) / 1024.0 << " KB" << endl;
    cout << "  " << get<0>( track_seek ) << endl;

    cout << "> Switch cost: " << get<3>( switch_seek ) / 1024.0 << " KB" << endl;
    if ( get<1>( switch_seek ).initialized() ) {
      cout << "  " << get<1>( switch_seek ).get() << endl;
    }
    cout << "  " << get<0>( switch_seek ) << endl;;



    if ( get<2>( track_seek ) <= get<3>( switch_seek ) and
         get<2>( track_seek ) < SIZE_MAX ) {
      follow_track_path( get<0>( track_seek ), get<1>( track_seek ) );
    }
    else if ( get<3>( switch_seek ) < SIZE_MAX ) {
      Optional<TrackPath> & extra_track_seek = get<1>( switch_seek );
      FrameDependencey & dependencies = get<2>( switch_seek );

      if ( extra_track_seek.initialized() ) {
        dependencies = follow_track_path( extra_track_seek.get(), dependencies );
      }

      follow_switch_path( get<0>( switch_seek ), dependencies );
    }
    else {
      throw runtime_error( "No path found." );
    }

    return cache_.get<RASTER, RasterHandle>( output_hash );
  }

  void print_cache()
  {
    cache_.print_cache();
  }
};

int main( int argc, char const *argv[] )
{
  try {
    if ( argc != 2 ) {
      cerr << "Usage: " << argv[ 0 ] << " <alf-video>" << endl;
      return EX_USAGE;
    }

    const string video_dir( argv[ 1 ] );
    PlayableAlfalfaVideo video( video_dir );
    FrameSeek frame_seek( video );
    FramePlayer player( video.get_info().width, video.get_info().height );
    VideoDisplay display( player.example_raster() );

    size_t step = 0;
    Optional<FrameInfo> current_frame;

    do {
      // Show current state information
      DecoderHash decoder_hash = player.get_decoder_hash();
      cout << "[" << step++ << "] " << decoder_hash.str() << endl;

      Optional<TrackData> next_frame_data = make_optional( false, TrackData( 0, 0, 0 ) );
      cout << "> j <n>\t" << "jump to frame (backward search, min bytes) <n>." << endl;

      // Is there a current frame? Show 'jump' and 'next frame' options.
      if ( current_frame.initialized() ) {
        next_frame_data = get_next_frame_data( video,
          current_frame.get().frame_id() );

        if ( next_frame_data.initialized() ) {
          cout << "> n\t" << "next raster in current track." << endl;
        }
      }

      // Applicable frames to current state
      auto search_it = video.get_frames_by_decoder_hash( decoder_hash );
      vector<FrameInfo> applicable_frames( search_it.first, search_it.second );

      if ( applicable_frames.size() > 0 ) {
        cout << "> a\t" << "list of all applicable frames ("
             << applicable_frames.size() << ")." << endl;
      }
      else {
        cout << "!! no applicable frames to the current state." << endl;
      }

      // Exit
      cout << "> q\t" << "quit." << endl;

      bool succeeded = false;

      // Showing the menu to the user
      while ( not succeeded ) {
        cout << ">> ";

        string command;
        getline( cin, command );
        istringstream ss( command );

        string arg0;
        ss >> arg0;

        switch ( arg0[ 0 ] ) {
        case 'j': // jump to frame <n>
          {
            if ( command.size() < 2 ) break;

            size_t jump_index;
            ss >> jump_index;

            vector<pair<size_t, double> > target_frames = find_target_frames( video, jump_index );

            if ( target_frames.size() == 0 ) {
              cout << "!! no frame with this index found." << endl;
              break;
            }

            size_t target_frame_index = 0;
            cout << endl;
            frame_seek.print_cache();
            cout << endl;

            for ( auto target_frame = target_frames.begin();
                  target_frame != target_frames.end(); target_frame++ ) {
              cout << target_frame_index++ << ") "
                   << "Q: " << target_frame->second << " / "
                   << video.get_frame( target_frame->first ).name().str() << endl;
            }

            cout << "[0-" << target_frames.size() - 1 << "]? ";
            cin >> target_frame_index;
            cin.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );

            if ( target_frame_index >= target_frames.size() ) {
              break;
            }

            unordered_set<DecoderHash> cached_states;
            cached_states.insert( decoder_hash );

            size_t desired_output = video.get_frame( target_frames[ target_frame_index ].first ).target_hash().output_hash;

            RasterHandle raster = frame_seek.get_raster( jump_index, desired_output );
            display_frame( display, make_optional( true, raster ) );

            /*
            vector<pair<size_t, pair<vector<size_t>, size_t> > > paths_list;

            auto paths = search_backward_dijkstra( video, adj_list,
              cached_states, video.frame_db()[ target_frames[ target_frame_index ].first ] );

            if ( paths.size() == 0 ) {
              cout << "!! no path found to raster " << jump_index << "." << endl;
              break;
            }

            size_t path_index = 0;
            cout << endl;
            for ( auto path = paths.begin(); path != paths.end(); path++ ) {
              paths_list.push_back( *path );
              cout << path_index++ << ") "
                   << "Hops: " << path->second.first.size() << "\t"
                   << "Cost: " << path->second.second / 1024.0 << " KB" << endl;
            }

            cout << "[0-" << paths_list.size() - 1 << "]? ";
            cin >> path_index;
            cin.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );

            if ( path_index >= paths_list.size() ) {
              break;
            }

            auto path = paths_list[ path_index ].second;

            for ( auto frame_id : path.first ) {
              const FrameInfo & frm = video.frame_db().search_by_frame_id( frame_id );
              Optional<RasterHandle> raster = player.decode( video.get_chunk( frm ) );

              if ( not display_frame( display, raster ) ) {
                cout << "!! unshown frame." << endl;
              }

              current_frame = make_optional( true, frm );
            }*/

            succeeded = true;
          }

          break;

        case 'n': // next raster in the current track
          if ( next_frame_data.initialized() ) {
            const FrameInfo & frm = video.get_frame( next_frame_data.get().frame_id );

            Optional<RasterHandle> raster = player.decode( video.get_chunk( frm ) );

            if ( not display_frame( display, raster ) ) {
              cout << "!! unshown frame." << endl;
            }

            current_frame = make_optional( true, frm );
            succeeded = true;
          }

          break;

        case 'a': // list of all applicable frames
          {
            if ( applicable_frames.size() == 0) break;

            cout << endl << "Applicable frames:" << endl;

            size_t applicable_frame_index = 0;
            for ( auto const & fi : applicable_frames ) {
              cout << applicable_frame_index++ << ") " << fi.name().str() << endl;
            }

            cout << "[0-" << applicable_frames.size() - 1 << "]? ";
            cin >> applicable_frame_index;
            cin.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );

            if ( applicable_frame_index < applicable_frames.size() ) {
              Optional<RasterHandle> raster = player.decode(
                video.get_chunk( applicable_frames[ applicable_frame_index ] ) );

              if ( not display_frame( display, raster ) ) {
                cout << "!! unshown frame." << endl;
              }

              current_frame = make_optional( true, applicable_frames[ applicable_frame_index ] );
            }
            else {
              break;
            }
          }

          succeeded = true;
          break;

        case 'q':
          return EXIT_SUCCESS;

        default:
          break;
        }

        if ( not succeeded ) {
          cout << "! invalid command." << endl;
        }
      }

      cout << endl;
    } while ( true );
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
