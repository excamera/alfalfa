#ifndef ALFALFA_PLAYER_HH
#define ALFALFA_PLAYER_HH

#include <map>
#include <set>
#include <tuple>
#include <vector>
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

template <class ObjectType>
class LRUCache
{
private:
  static constexpr const size_t cache_capacity = 32;

  std::list<size_t> cached_items_ {};
  std::map<size_t, std::pair<ObjectType,
                             std::list<size_t>::const_iterator> > cache_ {};

public:
  void put( const size_t key, const ObjectType & obj );
  bool has( const size_t key ) const;
  ObjectType get( const size_t key );

  void clear();
  size_t size() const;

  void print_cache() const;
};

class RasterAndStateCache
{
private:
  LRUCache<RasterHandle> raster_cache_ {};
  LRUCache<DecoderState> state_cache_ {};

public:
  void put( const Decoder & decoder );

  LRUCache<RasterHandle> & raster_cache() { return raster_cache_; }
  const LRUCache<RasterHandle> & raster_cache() const { return raster_cache_; }

  LRUCache<DecoderState> & state_cache() { return state_cache_; }
  const LRUCache<DecoderState> & state_cache() const { return state_cache_; }

  size_t size() const;

  void clear();
  void print_cache() const;
};

class AlfalfaPlayer
{
private:
  AlfalfaVideoClient video_;
  RasterAndStateCache cache_;

  /* Current number of bytes in frame buffer -- used for optimal track determination. */
  size_t downloaded_frame_bytes_;
  /* Sequence of frames currently being played by the player -- could be frames
     on a track, frames on a switch, or both. */
  std::vector<FrameInfo> current_track_;
  /* Index of next frame in current_track_ that needs to be downloaded. */
  size_t current_track_index_;

  LRUCache<Chunk> frame_cache_ {};

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

    void update_dependencies( const FrameInfo & frame, RasterAndStateCache & cache );
    void update_dependencies_forward( const FrameInfo & frame, RasterAndStateCache & cache );

    bool all_resolved() const;

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
  AlfalfaPlayer( const std::string & server_address );

  Optional<RasterHandle> get_raster( const size_t output_hash,
                                     PathType path_type = MINIMUM_PATH, bool verbose = false );

  const VP8Raster & example_raster();

  /* Helper method to easily set current_track. */
  void set_current_track( const size_t track_id, const size_t frame_index );
  /* Move current pointer position up by 1, and retrieve chunk from server.
     Also store chunk in local client-side cache, evicting element if needed. */
  Chunk get_next_chunk();
  /* Determine if it's possible to play the provided sequence of frames, given the
     provided throughput estimate. */
  bool determine_feasibility( const std::vector<FrameInfo> prospective_track,
                              const size_t throughput_estimate,
                              const size_t switching_track_index );

  size_t cache_size() { return cache_.size(); }
  void clear_cache();

  void print_cache() const;
};

#endif /* ALFALFA_PLAYER_HH */
