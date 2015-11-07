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
  double frame_rate( void ) const { return info_.frame_rate(); }
  uint32_t frame_count( void ) const { return info_.frame_count; }
  double frame_rate_numerator( void ) const { return info_.frame_rate_numerator; }
  double frame_rate_denominator( void ) const { return info_.frame_rate_denominator; }

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
struct RasterListRandomAccessTag;
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
    random_access
    <
      tag<RasterListRandomAccessTag>
    >,
    sequenced
    <
      tag<RasterListSequencedTag>
    >
  >
> RasterListCollection;

typedef RasterListCollection::index<RasterListByHashTag>::type
RasterListCollectionByHash;
typedef RasterListCollection::index<RasterListRandomAccessTag>::type
RasterListCollectionRandomAccess;

class RasterList : public BasicDatabase<RasterData, AlfalfaProtobufs::RasterData,
  RasterListCollection, RasterListSequencedTag>
{
public:
  RasterList( const std::string & filename, const std::string & magic_number, OpenMode mode = OpenMode::READ )
  : BasicDatabase<RasterData, AlfalfaProtobufs::RasterData,
    RasterListCollection, RasterListSequencedTag>( filename, magic_number, mode )
  {}

  bool has( const size_t & raster_hash ) const;
  size_t raster( size_t raster_index );
  bool operator==( const RasterList & other ) const;
  bool operator!=( const RasterList & other ) const;
};

/*
 * Quality DB
 *   sequence of original raster / approximate raster / quality
 */

struct QualityDBByOriginalAndApproximateRasterTag;
struct QualityDBByOriginalRasterTag;
struct QualityDBByApproximateRasterTag;
struct QualityDBSequencedTag;

typedef multi_index_container
<
  QualityData,
  indexed_by
  <
    hashed_unique
    <
      tag<QualityDBByOriginalAndApproximateRasterTag>,
      composite_key
      <
        QualityData,
        member<QualityData, size_t, &QualityData::original_raster>,
        member<QualityData, size_t, &QualityData::approximate_raster>
      >
    >,
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

typedef QualityDBCollection::index<QualityDBByOriginalAndApproximateRasterTag>::type
QualityDBCollectionByOriginalAndApproximateRaster;
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

  QualityData
  search_by_original_and_approximate_raster( const size_t & original_raster, const size_t & approximate_raster );
  std::pair<QualityDBCollectionByOriginalRaster::iterator, QualityDBCollectionByOriginalRaster::iterator>
  search_by_original_raster( const size_t & original_raster );
  std::pair<QualityDBCollectionByApproximateRaster::iterator, QualityDBCollectionByApproximateRaster::iterator>
  search_by_approximate_raster( const size_t & approximate_raster );
};

/*
 * Track DB
 *   mapping from ID to { sequence of frames }
 */

struct TrackDBByTrackIdAndFrameIndexTag;
struct TrackDBByTrackIdAndFrameIdTag;
struct TrackDBSequencedTag;

typedef multi_index_container
<
  TrackData,
  indexed_by
  <
    ordered_unique
    <
      tag<TrackDBByTrackIdAndFrameIndexTag>,
      composite_key
      <
        TrackData,
        member<TrackData, size_t, &TrackData::track_id>,
        member<TrackData, size_t, &TrackData::frame_index>
      >
    >,
    sequenced
    <
      tag<TrackDBSequencedTag>
    >
  >
> TrackDBCollection;

typedef TrackDBCollection::index<TrackDBByTrackIdAndFrameIndexTag>::type
TrackDBCollectionByTrackIdAndFrameIndex;

class TrackDB : public BasicDatabase<TrackData, AlfalfaProtobufs::TrackData,
  TrackDBCollection, TrackDBSequencedTag>
{
public:
  TrackDB( const std::string & filename, const std::string & magic_number, OpenMode mode = OpenMode::READ )
  : BasicDatabase<TrackData, AlfalfaProtobufs::TrackData,
    TrackDBCollection, TrackDBSequencedTag>( filename, magic_number, mode )
  {}

  std::pair<TrackDBCollectionByTrackIdAndFrameIndex::iterator, TrackDBCollectionByTrackIdAndFrameIndex::iterator>
  search_by_track_id( const size_t & track_id );
  bool has_track_id( const size_t & track_id );
  Optional<TrackData>
  search_next_frame( const size_t & track_id, const size_t & frame_index );
  void merge( const TrackDB & db, map<size_t, size_t> & frame_id_mapping );
};

#endif /* MANIFESTS_HH */
