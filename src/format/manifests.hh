#ifndef MANIFESTS_HH
#define MANIFESTS_HH

#include <set>
#include <map>
#include <string>
#include <vector>

#include "db.hh"
#include "dependency_tracking.hh"
#include "serialization.hh"
#include "protobufs/alfalfa.pb.h"

/*
 * VideoManifest
 */

class VideoManifest : public SerializableData
{
private:
  VideoInfo info_;

public:
  VideoManifest( const std::string & filename, const std::string & magic_number,
    OpenMode mode = OpenMode::READ );

  const std::string & fourcc( void ) const { return info_.fourcc; }
  uint16_t width( void ) const { return info_.width; }
  uint16_t height( void ) const { return info_.height; }
  uint32_t frame_rate( void ) const { return info_.frame_rate; }
  uint32_t time_scale( void ) const { return info_.time_scale; }
  uint32_t frame_count( void ) const { return info_.frame_count; }

  const VideoInfo & info() const { return info_; }
  void set_info( const VideoInfo & info );

  bool serialize( const std::string & filename = "" );
  bool deserialize( const std::string & filename = "" );

  bool good() const { return good_; }
};

/*
 * Raster List
 *  sequence of hashes of the original rasters that the video approximates
 */

struct RasterListByHashTag;
struct RasterListSequencedTag;

typedef multi_index_container
<
  RasterData,
  indexed_by
  <
    ordered_unique
    <
      tag<RasterListByHashTag>,
      member<RasterData, size_t, &RasterData::hash>
    >,
    sequenced
    <
      tag<RasterListSequencedTag>
    >
  >
> RasterListCollection;

typedef RasterListCollection::index<RasterListByHashTag>::type
RasterListCollectionByHash;

class RasterList : public BasicDatabase<RasterData, AlfalfaProtobufs::RasterData,
  RasterListCollection, RasterListSequencedTag>
{
public:
  RasterList( const std::string & filename, const std::string & magic_number, OpenMode mode = OpenMode::READ )
  : BasicDatabase<RasterData, AlfalfaProtobufs::RasterData,
    RasterListCollection, RasterListSequencedTag>( filename, magic_number, mode )
  {}

  bool has( const size_t & raster_hash ) const;
  bool operator==( const RasterList & other ) const;
  bool operator!=( const RasterList & other ) const;
};

/*
 * Quality DB
 *   sequence of original raster / approximate raster / quality
 */

struct QualityDBByOriginalRasterTag;
struct QualityDBByApproximateRasterTag;
struct QualityDBSequencedTag;

typedef multi_index_container
<
  QualityData,
  indexed_by
  <
    hashed_non_unique
    <
      tag<QualityDBByOriginalRasterTag>,
      member<QualityData, size_t, &QualityData::original_raster>
    >,
    hashed_non_unique
    <
      tag<QualityDBByApproximateRasterTag>,
      member<QualityData, size_t, &QualityData::approximate_raster>
    >,
    sequenced
    <
      tag<QualityDBSequencedTag>
    >
  >
> QualityDBCollection;

typedef QualityDBCollection::index<QualityDBByOriginalRasterTag>::type
QualityDBCollectionByOriginalRaster;
typedef QualityDBCollection::index<QualityDBByApproximateRasterTag>::type
QualityDBCollectionByApproximateRaster;

class QualityDB : public BasicDatabase<QualityData, AlfalfaProtobufs::QualityData,
  QualityDBCollection, QualityDBSequencedTag>
{
public:
  QualityDB( const std::string & filename, const std::string & magic_number, OpenMode mode = OpenMode::READ )
  : BasicDatabase<QualityData, AlfalfaProtobufs::QualityData,
    QualityDBCollection, QualityDBSequencedTag>( filename, magic_number, mode )
  {}

  std::pair<QualityDBCollectionByOriginalRaster::iterator, QualityDBCollectionByOriginalRaster::iterator>
  search_by_original_raster( const size_t & original_raster );
  std::pair<QualityDBCollectionByApproximateRaster::iterator, QualityDBCollectionByApproximateRaster::iterator>
  search_by_approximate_raster( const size_t & approximate_raster );
};

/*
 * Track DB
 *   mapping from ID to { sequence of frames }
 */

struct TrackDBByIdsTag;
struct TrackDBByFrameAndTrackIdTag;
struct TrackDBSequencedTag;

typedef multi_index_container
<
  TrackData,
  indexed_by
  <
    ordered_unique
    <
      tag<TrackDBByIdsTag>,
      composite_key
      <
        TrackData,
        member<TrackData, size_t, &TrackData::track_id>,
        member<TrackData, size_t, &TrackData::frame_id>
      >
    >,
    hashed_non_unique
    <
      tag<TrackDBByFrameAndTrackIdTag>,
      composite_key
      <
        TrackData,
        member<TrackData, size_t, &TrackData::track_id>,
        member<TrackData, SourceHash, &TrackData::source_hash>,
        member<TrackData, TargetHash, &TrackData::target_hash>
      >,
      composite_key_hash
      <
        std::hash<size_t>,
        std::hash<SourceHash>,
        std::hash<TargetHash>
      >,
      composite_key_equal_to
      <
        std::equal_to<size_t>,
        std::equal_to<SourceHash>,
        std::equal_to<TargetHash>
      >
    >,
    sequenced
    <
      tag<TrackDBSequencedTag>
    >
  >
> TrackDBCollection;

typedef TrackDBCollection::index<TrackDBByIdsTag>::type
TrackDBCollectionByIds;
typedef TrackDBCollection::index<TrackDBByFrameAndTrackIdTag>::type
TrackDBCollectionByFrameAndTrackId;

class TrackDB : public BasicDatabase<TrackData, AlfalfaProtobufs::TrackData,
  TrackDBCollection, TrackDBSequencedTag>
{
public:
  TrackDB( const std::string & filename, const std::string & magic_number, OpenMode mode = OpenMode::READ )
  : BasicDatabase<TrackData, AlfalfaProtobufs::TrackData,
    TrackDBCollection, TrackDBSequencedTag>( filename, magic_number, mode )
  {}

  std::pair<TrackDBCollectionByIds::iterator, TrackDBCollectionByIds::iterator>
  search_by_track_id( const size_t track_id );
  Optional<TrackData>
  search_next_frame( const size_t track_id, const SourceHash source_hash, const TargetHash target_hash );
  void merge( const TrackDB & db );
};

#endif /* MANIFESTS_HH */
