#include "alfalfa_player.hh"

using namespace std;

// Used to control the maximum number of FrameInfos batched into a single
// AlfalfaProtobufs::FrameIterator object -- this is to ensure protobufs
// sent over the network aren't too large.
const size_t MAX_NUM_FRAMES = 1000;

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

void LRUCache::clear()
{
  cache_.clear();
  cached_items_.clear();
}

size_t LRUCache::size() const
{
  return cache_.size();
}

void LRUCache::print_cache() const
{
  for ( auto const & item : cache_ )
  {
    if ( item.first.first == RASTER ) {
      cout << "R ";
    }
    else {
      cout << "S ";
    }

    cout << hex << uppercase << item.first.second << dec << nouppercase << endl;
  }
}

template<DependencyType DepType>
size_t AlfalfaPlayer::FrameDependency::increase_count( const size_t hash )
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
size_t AlfalfaPlayer::FrameDependency::decrease_count( const size_t hash )
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
size_t AlfalfaPlayer::FrameDependency::get_count( const size_t hash ) const
{
  DependencyVertex vertex{ DepType, hash };

  const auto & ref_count = ref_counter_.find( vertex );
  if ( ref_count == ref_counter_.end() ) {
    return 0;
  } else {
    return ref_count->second;
  }
}

void AlfalfaPlayer::FrameDependency::update_dependencies( const FrameInfo & frame,
                                                           LRUCache & cache )
{
  unresolved_.erase( DependencyVertex{ RASTER, frame.target_hash().output_hash } );
  unresolved_.erase( DependencyVertex{ STATE, frame.target_hash().state_hash } );

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
}

void AlfalfaPlayer::FrameDependency::update_dependencies_forward( const FrameInfo & frame,
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

bool AlfalfaPlayer::FrameDependency::all_resolved()
{
  return unresolved_.size() == 0;
}

tuple<AlfalfaPlayer::SwitchPath, Optional<AlfalfaPlayer::TrackPath>,
AlfalfaPlayer::FrameDependency>
AlfalfaPlayer::get_min_switch_seek( const size_t output_hash )
{
  auto frames = video_.get_frames_by_output_hash( output_hash );

  size_t min_cost = SIZE_MAX;
  SwitchPath min_switch_path;
  min_switch_path.cost = SIZE_MAX;
  Optional<TrackPath> min_track_path;
  FrameDependency min_dependencies;

  for ( auto target_frame : frames ) {
    auto switches = video_.get_switches_ending_with_frame( target_frame.frame_id() );

    for ( auto sw : switches ) {
      size_t cost = 0;
      FrameDependency dependencies;

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

tuple<size_t, AlfalfaPlayer::FrameDependency, size_t>
AlfalfaPlayer::get_track_seek( const size_t track_id, const size_t from_frame_index,
                               FrameDependency dependencies )
{
  int cur_frame_index = (int) from_frame_index;
  // Since range specified in get_frames_reverse is inclusive on both sides, add 1 to
  // to_frame_index.
  size_t to_frame_index = ( cur_frame_index - (int) MAX_NUM_FRAMES + 1 ) >= 0 ?
    ( from_frame_index - MAX_NUM_FRAMES + 1 ) : 0;
  auto frames_backward = video_.get_frames_reverse( track_id, from_frame_index, to_frame_index );

  if ( frames_backward.size() == 0 ) {
    return make_tuple( -1, dependencies, SIZE_MAX );
  }

  size_t cost = 0;
  while ( cur_frame_index >= 0 ) {
    for ( auto frame : frames_backward ) {
      cost += frame.length();

      dependencies.update_dependencies( frame, cache_ );

      if ( dependencies.all_resolved() ) {
        return make_tuple( (size_t) cur_frame_index, dependencies, cost );
      }
      cur_frame_index--;
    }
    if ( cur_frame_index >= 0 ) {
      to_frame_index = ( cur_frame_index - (int) MAX_NUM_FRAMES + 1 ) >= 0 ?
        ( from_frame_index - MAX_NUM_FRAMES + 1 ) : 0;
      frames_backward = video_.get_frames_reverse( track_id, (size_t) cur_frame_index, to_frame_index );
    }
  }

  return make_tuple( from_frame_index, dependencies, SIZE_MAX );
}

tuple<AlfalfaPlayer::TrackPath, AlfalfaPlayer::FrameDependency>
AlfalfaPlayer::get_min_track_seek( const size_t output_hash )
{
  size_t min_cost = SIZE_MAX;
  TrackPath min_track_path;
  min_track_path.cost = SIZE_MAX;

  FrameDependency min_frame_dependency;

  auto track_ids = video_.get_track_ids();

  for ( auto frame : video_.get_frames_by_output_hash( output_hash ) ) {
    for ( auto track_data : video_.get_track_data_by_frame_id( frame.frame_id() ) ) {
      tuple<size_t, FrameDependency, size_t> seek =
        get_track_seek( track_data.track_id, track_data.frame_index );

      if ( get<2>( seek ) < min_cost ) {
        min_cost = get<2>( seek );
        min_track_path = TrackPath{ track_data.track_id, get<0>( seek ),
                                    track_data.frame_index + 1,
                                    min_cost };
        min_frame_dependency = get<1>( seek );
      }
    }
  }

  return make_tuple( min_track_path, min_frame_dependency );
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

AlfalfaPlayer::FrameDependency AlfalfaPlayer::follow_track_path( TrackPath path,
                                                                  FrameDependency dependencies )
{
  References refs( video_.get_video_width(), video_.get_video_height() );
  DecoderState state( video_.get_video_width(), video_.get_video_height() );

  size_t from_frame_index = path.start_index;
  size_t to_frame_index = ( from_frame_index + MAX_NUM_FRAMES ) >= path.end_index ?
    path.end_index : ( from_frame_index + MAX_NUM_FRAMES );
  auto frames = video_.get_frames( path.track_id, from_frame_index, to_frame_index );

  while ( from_frame_index < path.end_index ) {
    for ( auto frame : frames ) {
      Decoder && decoder = get_decoder( frame );
      pair<bool, RasterHandle> output = decoder.get_frame_output( video_.get_chunk( frame ) );
      cache_.put( decoder );
      cache_.put<RASTER>( output.second );
      dependencies.update_dependencies_forward( frame, cache_ );
    }
    from_frame_index += MAX_NUM_FRAMES;
    if ( from_frame_index < path.end_index ) {
      to_frame_index = ( from_frame_index + MAX_NUM_FRAMES ) >= path.end_index ?
        path.end_index : ( from_frame_index + MAX_NUM_FRAMES );
      frames = video_.get_frames( path.track_id, from_frame_index, to_frame_index );
    }
  }

  return dependencies;
}

AlfalfaPlayer::FrameDependency AlfalfaPlayer::follow_switch_path( SwitchPath path,
                                                                   FrameDependency dependencies)
{
  References refs( video_.get_video_width(), video_.get_video_height() );
  DecoderState state( video_.get_video_width(), video_.get_video_height() );

  auto frames = video_.get_frames( path.from_track_id, path.to_track_id,
    path.from_frame_index, path.switch_start_index, path.switch_end_index );

  for ( auto frame : frames ) {
    Decoder && decoder = get_decoder( frame );
    pair<bool, RasterHandle> output = decoder.get_frame_output( video_.get_chunk( frame ) );
    cache_.put( decoder );
    cache_.put<RASTER>( output.second );
    dependencies.update_dependencies_forward( frame, cache_ );
  }

  return dependencies;
}

Optional<RasterHandle> AlfalfaPlayer::get_raster_track_path( const size_t output_hash )
{
  auto track_seek = get_min_track_seek( output_hash );

  if ( get<0>( track_seek ).cost == SIZE_MAX) {
    return Optional<RasterHandle>();
  }

  follow_track_path( get<0>( track_seek ), get<1>( track_seek ) );
  return make_optional<RasterHandle>( true, cache_.get<RASTER, RasterHandle>( output_hash ) );
}

Optional<RasterHandle> AlfalfaPlayer::get_raster_switch_path( const size_t output_hash )
{
  auto switch_seek = get_min_switch_seek( output_hash );

  if ( get<0>( switch_seek ).cost == SIZE_MAX ) {
    return Optional<RasterHandle>();
  }

  Optional<TrackPath> & extra_track_seek = get<1>( switch_seek );
  FrameDependency & dependencies = get<2>( switch_seek );

  if ( extra_track_seek.initialized() ) {
    dependencies = follow_track_path( extra_track_seek.get(), dependencies );
  }

  follow_switch_path( get<0>( switch_seek ), dependencies );

  return make_optional<RasterHandle>( true, cache_.get<RASTER, RasterHandle>( output_hash ) );
}

Optional<RasterHandle> AlfalfaPlayer::get_raster( const size_t output_hash,
                                                  PathType path_type, bool verbose )
{
  if ( verbose ) {
    auto track_seek = get_min_track_seek( output_hash );
    auto switch_seek = get_min_switch_seek( output_hash );

    if ( get<0>( track_seek ).cost < SIZE_MAX ) {
      cout << "> Track seek:" << endl << get<0>( track_seek ) << endl;
    }

    if ( get<0>( switch_seek ).cost < SIZE_MAX ) {
      cout << "> Switch seek:" << endl;

      if ( get<1>( switch_seek ).initialized() ) {
        cout << get<1>( switch_seek ).get() << endl;
      }

      cout << get<0>( switch_seek ) << endl;
    }
  }

  switch( path_type ) {
  case TRACK_PATH:
  {
    Optional<RasterHandle> result = get_raster_track_path( output_hash );

    if ( not result.initialized() and verbose ) {
      cout << "No track paths found." << endl;
    }

    return result;
  }
  case SWITCH_PATH:
  {
    Optional<RasterHandle> result = get_raster_switch_path( output_hash );

    if ( not result.initialized() and verbose ) {
      cout << "No switch paths found." << endl;
    }

    return result;
  }

  case MINIMUM_PATH:
  default:
    auto track_seek = get_min_track_seek( output_hash );
    auto switch_seek = get_min_switch_seek( output_hash );

    if ( get<0>( track_seek ).cost <= get<0>( switch_seek ).cost ) {
      return get_raster( output_hash, TRACK_PATH, false );
    }
    else {
      return get_raster( output_hash, SWITCH_PATH, false );
    }
  }
}

const VP8Raster & AlfalfaPlayer::example_raster()
{
  Decoder temp( video_.get_video_width(), video_.get_video_height() );
  return temp.example_raster();
}

void AlfalfaPlayer::clear_cache()
{
  cache_.clear();
}

void AlfalfaPlayer::print_cache()
{
  cache_.print_cache();
}
