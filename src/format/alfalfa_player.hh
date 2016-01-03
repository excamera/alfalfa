#ifndef ALFALFA_PLAYER_HH
#define ALFALFA_PLAYER_HH

#include <map>
#include <set>
#include <tuple>
#include <vector>
#include <boost/variant.hpp>
#include <boost/format.hpp>

#include "alfalfa_video_client.hh"

enum DependencyType
{
  STATE,
  RASTER
};

enum PathType
{
  MINIMUM_PATH,
  TRACK_PATH,
  SWITCH_PATH
};

using DependencyVertex = std::pair<DependencyType, size_t /* hash */>;

class LRUCache
{
private:
  size_t cache_size_ = { 32 };

  std::list<DependencyVertex> cached_items_ = {};
  std::map<DependencyVertex, std::pair<boost::variant<DecoderState, RasterHandle>,
                                       std::list<DependencyVertex>::const_iterator> > cache_ = {};


public:
  template<DependencyType DepType, class ObjectType>
  void put( ObjectType obj );

  void put( const Decoder & decoder );

  template<DependencyType DepType>
  bool has( size_t hash ) const;

  template<DependencyType DepType>
  void remove( size_t hash );

  template<DependencyType DepType, class ObjectType>
  ObjectType get( size_t hash );

  void clear();
  size_t size();

  void print_cache();
};

class AlfalfaPlayer
{
private:
  AlfalfaVideoClient video_;
  LRUCache cache_;

  class FrameDependency
  {
  private:
    std::map<DependencyVertex, size_t> ref_counter_ = {};
    std::set<DependencyVertex> unresolved_ = {};

  public:
    template<DependencyType DepType>
    size_t increase_count( const size_t hash );

    template<DependencyType DepType>
    size_t decrease_count( const size_t hash );

    template<DependencyType DepType>
    size_t get_count( const size_t hash ) const;

    void update_dependencies( const FrameInfo & frame, LRUCache & cache );
    void update_dependencies_forward( const FrameInfo & frame, LRUCache & cache );

    bool all_resolved();

    FrameDependency() {}
  };

  struct TrackPath
  {
    size_t track_id;
    size_t start_index;
    size_t end_index;

    size_t cost;

    friend std::ostream & operator<<( std::ostream & os, const TrackPath & path )
    {
      os << boost::format( "Track %-d: %-d -> %-d (%-.2f KB)" ) % path.track_id
                                                                   % path.start_index
                                                                   % ( path.end_index - 1 )
                                                                   % ( path.cost / 1024.0 );

      return os;
    }
  };

  struct SwitchPath
  {
    size_t from_track_id;
    size_t from_frame_index;
    size_t to_track_id;
    size_t to_frame_index;
    size_t switch_start_index;
    size_t switch_end_index;

    size_t cost;

    friend std::ostream & operator<<( std::ostream & os, const SwitchPath & path )
    {
      os << boost::format( "Switch: Track %-d{%-d} -> Track %-d{%-d} [%-d:%-d] (%-.2f KB)" )
            % path.from_track_id
            % path.from_frame_index
            % path.to_track_id
            % path.to_frame_index
            % path.switch_start_index
            % ( path.switch_end_index - 1 )
            % ( path.cost / 1024.0 );

      return os;
    }
  };

  std::tuple<SwitchPath, Optional<TrackPath>, FrameDependency>
  get_min_switch_seek( const size_t output_hash );

  std::tuple<size_t, FrameDependency, size_t>
  get_track_seek( const size_t track_id, const size_t frame_index,
                  FrameDependency dependencies = {} );

  std::tuple<TrackPath, FrameDependency>
  get_min_track_seek( const size_t output_hash );

  Decoder get_decoder( const FrameInfo & frame );
  FrameDependency follow_track_path( TrackPath path, FrameDependency dependencies );
  FrameDependency follow_switch_path( SwitchPath path, FrameDependency dependencies);

  Optional<RasterHandle> get_raster_switch_path( const size_t output_hash );
  Optional<RasterHandle> get_raster_track_path( const size_t output_hash );

public:
  AlfalfaPlayer( const std::string & directory_name );

  Optional<RasterHandle> get_raster( const size_t output_hash,
                                     PathType path_type = MINIMUM_PATH, bool verbose = false );

  const VP8Raster & example_raster();

  size_t cache_size() { return cache_.size(); }
  void clear_cache();

  void print_cache();
};

#endif /* ALFALFA_PLAYER_HH */
