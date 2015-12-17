#include "alfalfa_player.hh"

using namespace std;

template<DependencyType DepType, class ObjectType>
void CacheManager::do_cache( ObjectType obj )
{
  cache_.insert( make_pair( DependencyVertex{ DepType, obj.hash() }, obj ) );
}

void CacheManager::do_cache( const Decoder & decoder )
{
  do_cache<RASTER>( decoder.get_references().last );
  do_cache<RASTER>( decoder.get_references().golden );
  do_cache<RASTER>( decoder.get_references().alternative_reference );
  do_cache<STATE>( decoder.get_state() );
}

template<DependencyType DepType>
bool CacheManager::has( size_t hash ) const
{
  return cache_.count( DependencyVertex{ DepType, hash } ) > 0;
}

template<DependencyType DepType>
void CacheManager::remove( size_t hash )
{
  DependencyVertex vertex{ DepType, hash };
  cache_.erase( vertex );
}

template<DependencyType DepType, class ObjectType>
ObjectType CacheManager::get( size_t hash )
{
  DependencyVertex vertex{ DepType, hash };
  return boost::get<ObjectType>( cache_.at( vertex ) );
}

void CacheManager::print_cache()
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
                                                           CacheManager & cache )
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
                                                                   CacheManager & cache )
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

  for ( auto target_frame = frames.first; target_frame != frames.second; target_frame++ ) {
    auto switches = video_.get_switches_ending_with_frame( target_frame->frame_id() );

    for ( auto sw = switches.begin(); sw != switches.end(); sw++ ) {
      size_t cost = 0;
      FrameDependencey dependencies;

      for ( auto frame = sw->first; frame != sw->second; frame-- ) {
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
          min_switch_path.to_frame_index = sw->first.to_frame_index();
          min_switch_path.switch_start_index = 0;
          min_switch_path.switch_end_index = sw->first.switch_frame_index();
          min_switch_path.cost = min_cost;

          min_track_path.clear();
          min_track_path.initialize( TrackPath{ sw->first.from_track_id(),
            get<0>( track_seek ).frame_index(), sw->first.from_frame_index(),
            get<2>( track_seek ) } );
          min_dependencies = get<1>( track_seek );
        }
      }
      else {
        if ( cost < min_cost ) {
          min_cost = cost;

          min_switch_path.from_track_id = sw->first.from_track_id();
          min_switch_path.to_track_id = sw->first.to_track_id();
          min_switch_path.from_frame_index = sw->first.from_frame_index();
          min_switch_path.to_frame_index = sw->first.to_frame_index();
          min_switch_path.switch_start_index = 0;
          min_switch_path.switch_end_index = sw->first.switch_frame_index();
          min_switch_path.cost = min_cost;

          min_dependencies = dependencies;
        }
      }
    }
  }

  return make_tuple( min_switch_path, min_track_path, min_dependencies );
}

tuple<TrackDBIterator, AlfalfaPlayer::FrameDependencey, size_t>
AlfalfaPlayer::get_track_seek( const size_t track_id, const size_t frame_index,
                               FrameDependencey dependencies )
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

tuple<AlfalfaPlayer::TrackPath, AlfalfaPlayer::FrameDependencey>
AlfalfaPlayer::get_min_track_seek( const size_t frame_index, const size_t output_hash )
{
  size_t min_cost = SIZE_MAX;
  TrackPath min_track_path;
  min_track_path.cost = SIZE_MAX;

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
        min_track_path = TrackPath{ *track_id, get<0>( seek ).frame_index(), frame_index,
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

  for ( auto frame = frames.first; frame != frames.second; frame++ ) {
    Decoder && decoder = get_decoder( *frame );
    pair<bool, RasterHandle> output = decoder.get_frame_output( video_.get_chunk( *frame ) );
    cache_.do_cache<RASTER>( output.second );
    cache_.do_cache( decoder );
    dependencies.update_dependencies_forward( *frame, cache_ );
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

  for ( auto frame = frames.first; frame != frames.second; frame++ ) {
    Decoder && decoder = get_decoder( *frame );
    pair<bool, RasterHandle> output = decoder.get_frame_output( video_.get_chunk( *frame ) );
    cache_.do_cache<RASTER>( output.second );
    cache_.do_cache( decoder );
    dependencies.update_dependencies_forward( *frame, cache_ );
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
       get<0>( track_seek ).cost < SIZE_MAX and false ) {
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
