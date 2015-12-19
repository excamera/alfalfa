#ifndef ALFALFA_PLAYER_HH
#define ALFALFA_PLAYER_HH

#include <map>
#include <set>
#include <tuple>
#include <vector>
#include <boost/variant.hpp>

#include "alfalfa_video_client.hh"

enum DependencyType
{
  STATE,
  RASTER
};

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

  void print_cache();
};

class AlfalfaPlayer
{
private:
  AlfalfaVideoClient video_;
  LRUCache cache_;

  class FrameDependencey
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
    size_t get_count( const size_t hash );

    void update_dependencies( const FrameInfo & frame, LRUCache & cache );
    void update_dependencies_forward( const FrameInfo & frame, LRUCache & cache );

    bool all_resolved();
  };

  struct TrackPath
  {
    size_t track_id;
    size_t start_index;
    size_t end_index;

    size_t cost;

    friend std::ostream & operator<<( std::ostream & os, const TrackPath & path )
    {
      os << "Track " << path.track_id << ": " << path.start_index << " -> "
         << path.end_index - 1;

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
      os << "Switch: Track " << path.from_track_id << " (" << path.from_frame_index
         << ") -> Track " << path.to_track_id << " (" << path.to_frame_index
         << ") [" << path.switch_start_index << ":"
         << path.switch_end_index - 1 << "]";

      return os;
    }
  };

  std::tuple<SwitchPath, Optional<TrackPath>, FrameDependencey>
  get_min_switch_seek( const size_t output_hash );

  std::tuple<size_t, FrameDependencey, size_t>
  get_track_seek( const size_t track_id, const size_t frame_index,
                  FrameDependencey dependencies = {} );

  std::tuple<TrackPath, FrameDependencey>
  get_min_track_seek( const size_t frame_index, const size_t output_hash );

  Decoder get_decoder( const FrameInfo & frame );
  FrameDependencey follow_track_path( TrackPath path, FrameDependencey dependencies );
  FrameDependencey follow_switch_path( SwitchPath path, FrameDependencey dependencies);

public:
  AlfalfaPlayer( const std::string & directory_name );

  RasterHandle get_raster( const size_t frame_index, const size_t output_hash );
  const VP8Raster & example_raster();

  void print_cache();
};

#endif /* ALFALFA_PLAYER_HH */
