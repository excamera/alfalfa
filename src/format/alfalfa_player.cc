#include "alfalfa_player.hh"

using namespace std;

template<DependencyType DepType, class ObjectType>
void LRUCache::put( ObjectType obj )
{
  DependencyVertex vertex{ DepType, obj.hash() };

  if ( cache_.count( vertex ) > 0 ) {
    auto & item = cache_.at( vertex );
    cached_items_.erase( item.second );
    cached_items_.push_front( vertex );
    item.second = cached_items_.cbegin();
  }
  else {
    cached_items_.push_front( vertex );

    if ( cached_items_.size() > cache_size_ ) {
      cache_.erase( cached_items_.back() );
      cached_items_.pop_back();
    }
  }

  cache_.insert( make_pair( vertex,
                            make_pair( obj, cached_items_.cbegin() ) ) );
}

void LRUCache::put( const Decoder & decoder )
{
  put<RASTER>( decoder.get_references().last );
  put<RASTER>( decoder.get_references().golden );
  put<RASTER>( decoder.get_references().alternative_reference );
  put<STATE>( decoder.get_state() );
}

template<DependencyType DepType>
bool LRUCache::has( size_t hash ) const
{
  return cache_.count( DependencyVertex{ DepType, hash } ) > 0;
}

template<DependencyType DepType>
void LRUCache::remove( size_t hash )
{
  DependencyVertex vertex{ DepType, hash };
  cached_items_.erase( cache_.at( vertex ).second );
  cache_.erase( vertex );
}

template<DependencyType DepType, class ObjectType>
ObjectType LRUCache::get( size_t hash )
{
  DependencyVertex vertex{ DepType, hash };

  auto & item = cache_.at( vertex );
  cached_items_.erase( item.second );
  cached_items_.push_front( vertex );
  item.second = cached_items_.cbegin();

  return boost::get<ObjectType>( cache_.at( vertex ).first );
}

void LRUCache::print_cache()
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

template<DependencyType DepType>
size_t AlfalfaPlayer::FrameDependencey::increase_count( const size_t hash )
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
size_t AlfalfaPlayer::FrameDependencey::decrease_count( const size_t hash )
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
size_t AlfalfaPlayer::FrameDependencey::get_count( const size_t hash )
{
  DependencyVertex vertex{ DepType, hash };

  if ( ref_counter_.count( vertex ) ) {
    return ref_counter_[ vertex ];
  }

  return 0;
}

void AlfalfaPlayer::FrameDependencey::update_dependencies( const FrameInfo & frame,
                                                           LRUCache & cache )
{
  Optional<size_t> hash[] = {
    frame.source_hash().last_hash, frame.source_hash().golden_hash,
    frame.source_hash().alt_hash
  };

  for ( int i = 0; i < 3; i++ ) {
    if ( hash[ i ].initialized() ) {
      if ( not cache.has<RASTER>( hash[ i ].get() ) ) {
        increase_count<RASTER>( hash[ i ].get() );
        unresolved_.insert( DependencyVertex{ RASTER, hash[ i ].get() } );
      }
    }
  }

  if ( frame.source_hash().state_hash.initialized() ) {
    if ( not cache.has<STATE>( frame.source_hash().state_hash.get() ) ) {
      increase_count<STATE>( frame.source_hash().state_hash.get() );
      unresolved_.insert( DependencyVertex{ STATE,
        frame.source_hash().state_hash.get() } );
    }
  }

  unresolved_.erase( DependencyVertex{ RASTER, frame.target_hash().output_hash } );
  unresolved_.erase( DependencyVertex{ STATE, frame.target_hash().state_hash } );
}

void AlfalfaPlayer::FrameDependencey::update_dependencies_forward( const FrameInfo & frame,
                                                                   LRUCache & cache )
{
  Optional<size_t> hash[] = {
    frame.source_hash().last_hash, frame.source_hash().golden_hash,
    frame.source_hash().alt_hash
  };

  for ( int i = 0; i < 3; i++ ) {
    if ( hash[ i ].initialized() ) {
      if ( not cache.has<RASTER>( hash[ i ].get() ) ) {
        decrease_count<RASTER>( hash[ i ].get() );
      }
    }
  }

  if ( frame.source_hash().state_hash.initialized() ) {
    if ( not cache.has<STATE>( frame.source_hash().state_hash.get() ) ) {
      decrease_count<STATE>( frame.source_hash().state_hash.get() );
    }
  }
}

bool AlfalfaPlayer::FrameDependencey::all_resolved()
{
  return unresolved_.size() == 0;
}

tuple<AlfalfaPlayer::SwitchPath, Optional<AlfalfaPlayer::TrackPath>,
AlfalfaPlayer::FrameDependencey>
AlfalfaPlayer::get_min_switch_seek( const size_t output_hash )
{
  auto frames = video_.get_frames_by_output_hash( output_hash );

  size_t min_cost = SIZE_MAX;
  SwitchPath min_switch_path;
  min_switch_path.cost = SIZE_MAX;
  Optional<TrackPath> min_track_path;
  FrameDependencey min_dependencies;

  for ( auto target_frame : frames ) {
    auto switches = video_.get_switches_ending_with_frame( target_frame.frame_id() );

    for ( auto sw : switches ) {
      size_t cost = 0;
      FrameDependencey dependencies;

      size_t cur_switch_frame_index = sw.switch_start_index;
      for ( auto frame : sw.frames ) {
        cost += frame.length();

        dependencies.update_dependencies( frame, cache_ );

        if ( dependencies.all_resolved() ) {
          break;
        }
        cur_switch_frame_index++;
      }

      if ( not dependencies.all_resolved() ) {
        auto track_seek = get_track_seek( sw.from_track_id, sw.from_frame_index,
          dependencies );

        if ( get<2>( track_seek ) == SIZE_MAX ) {
          cost = SIZE_MAX;
          break;
        }

        cost += get<2>( track_seek );

        if ( cost < min_cost ) {
          min_cost = cost;

          min_switch_path.from_track_id = sw.from_track_id;
          min_switch_path.to_track_id = sw.to_track_id;
          min_switch_path.from_frame_index = sw.from_frame_index;
          min_switch_path.to_frame_index = sw.to_frame_index;
          min_switch_path.switch_start_index = 0;
          min_switch_path.switch_end_index = cur_switch_frame_index + 1;
          min_switch_path.cost = min_cost;

          min_track_path.clear();
          min_track_path.initialize( TrackPath{ sw.from_track_id,
            get<0>( track_seek ), sw.from_frame_index + 1,
            get<2>( track_seek ) } );
          min_dependencies = get<1>( track_seek );
        }
      }
      else {
        if ( cost < min_cost ) {
          min_cost = cost;

          min_switch_path.from_track_id = sw.from_track_id;
          min_switch_path.to_track_id = sw.to_track_id;
          min_switch_path.from_frame_index = sw.from_frame_index;
          min_switch_path.to_frame_index = sw.to_frame_index;
          min_switch_path.switch_start_index = 0;
          min_switch_path.switch_end_index = cur_switch_frame_index + 1;
          min_switch_path.cost = min_cost;

          min_dependencies = dependencies;
        }
      }
    }
  }

  return make_tuple( min_switch_path, min_track_path, min_dependencies );
}

tuple<size_t, AlfalfaPlayer::FrameDependencey, size_t>
AlfalfaPlayer::get_track_seek( const size_t track_id, const size_t frame_index,
                               FrameDependencey dependencies )
{
  auto frames_backward = video_.get_frames_reverse( track_id, frame_index );
  size_t cur_frame_index = frame_index;

  if ( frames_backward.size() == 0 ) {
    return make_tuple( -1, dependencies, SIZE_MAX );
  }

  size_t cost = 0;

  for( auto frame : frames_backward ) {
    cost += frame.length();

    dependencies.update_dependencies( frame, cache_ );

    if ( dependencies.all_resolved() ) {
      return make_tuple( cur_frame_index, dependencies, cost );
    }
    cur_frame_index--;
  }

  return make_tuple( frame_index, dependencies, SIZE_MAX );
}

tuple<AlfalfaPlayer::TrackPath, AlfalfaPlayer::FrameDependencey>
AlfalfaPlayer::get_min_track_seek( const size_t frame_index, const size_t output_hash )
{
  size_t min_cost = SIZE_MAX;
  TrackPath min_track_path;
  min_track_path.cost = SIZE_MAX;

  FrameDependencey min_frame_dependecy;

  auto track_ids = video_.get_track_ids();

  for ( auto track_id : track_ids )
  {
    auto track_frame = video_.get_frame( track_id, frame_index );
    auto frame = video_.get_frame( track_frame.frame_id );

    if ( frame.target_hash().output_hash == output_hash ) {
      tuple<size_t, FrameDependencey, size_t> seek = get_track_seek( track_id, frame_index );

      if ( get<2>( seek ) < min_cost ) {
        min_cost = get<2>( seek );
        min_track_path = TrackPath{ track_id, get<0>( seek ), frame_index + 1,
                                    min_cost };
        min_frame_dependecy = get<1>( seek );
      }
    }
  }

  return make_tuple( min_track_path, min_frame_dependecy );
}

AlfalfaPlayer::AlfalfaPlayer( const std::string & directory_name )
  : video_( directory_name ), cache_()
{}

Decoder AlfalfaPlayer::get_decoder( const FrameInfo & frame )
{
  References refs( video_.get_video_width(), video_.get_video_height() );
  DecoderState state( video_.get_video_width(), video_.get_video_height() );

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

AlfalfaPlayer::FrameDependencey AlfalfaPlayer::follow_track_path( TrackPath path,
                                                                  FrameDependencey dependencies )
{
  References refs( video_.get_video_width(), video_.get_video_height() );
  DecoderState state( video_.get_video_width(), video_.get_video_height() );

  auto frames = video_.get_frames( path.track_id, path.start_index, path.end_index );

  for ( auto frame : frames ) {
    Decoder && decoder = get_decoder( frame );
    pair<bool, RasterHandle> output = decoder.get_frame_output( video_.get_chunk( frame ) );
    cache_.put<RASTER>( output.second );
    cache_.put( decoder );
    dependencies.update_dependencies_forward( frame, cache_ );
  }

  return dependencies;
}

AlfalfaPlayer::FrameDependencey AlfalfaPlayer::follow_switch_path( SwitchPath path,
                                                                   FrameDependencey dependencies)
{
  References refs( video_.get_video_width(), video_.get_video_height() );
  DecoderState state( video_.get_video_width(), video_.get_video_height() );

  auto frames = video_.get_frames( path.from_track_id, path.to_track_id,
    path.from_frame_index, path.switch_start_index, path.switch_end_index );

  for ( auto frame : frames ) {
    Decoder && decoder = get_decoder( frame );
    pair<bool, RasterHandle> output = decoder.get_frame_output( video_.get_chunk( frame ) );
    cache_.put<RASTER>( output.second );
    cache_.put( decoder );
    dependencies.update_dependencies_forward( frame, cache_ );
  }

  return dependencies;
}

RasterHandle AlfalfaPlayer::get_raster( const size_t frame_index, const size_t output_hash )
{
  auto track_seek = get_min_track_seek( frame_index, output_hash );
  auto switch_seek = get_min_switch_seek( output_hash );

  if( get<0>( track_seek ).cost < SIZE_MAX ) {
    cout << "> Track cost:  " << get<0>( track_seek ).cost / 1024.0 << " KB" << endl;
    cout << "  " << get<0>( track_seek ) << endl;
  }

  if ( get<0>( switch_seek ).cost < SIZE_MAX ) {
    cout << "> Switch cost: " << get<0>( switch_seek ).cost / 1024.0 << " KB" << endl;
    if ( get<1>( switch_seek ).initialized() ) {
      cout << "  " << get<1>( switch_seek ).get() << " (cost: "
           << get<1>( switch_seek ).get().cost / 1024.0 << " KB)"
           << endl;
    }
    cout << "  " << get<0>( switch_seek ) << endl;
  }

  if ( get<0>( track_seek ).cost <= get<0>( switch_seek ).cost and
       get<0>( track_seek ).cost < SIZE_MAX ) {
    follow_track_path( get<0>( track_seek ), get<1>( track_seek ) );
  }
  else if ( get<0>( switch_seek ).cost < SIZE_MAX ) {
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

const VP8Raster & AlfalfaPlayer::example_raster()
{
  Decoder temp( video_.get_video_width(), video_.get_video_height() );
  return temp.example_raster();
}

void AlfalfaPlayer::print_cache()
{
  cache_.print_cache();
}
