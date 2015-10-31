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

  bool has( const size_t & raster_hash ) const
  {
    const RasterListCollectionByHash & data_by_hash = collection_.get<RasterListByHashTag>();
    return data_by_hash.count( raster_hash ) > 0;
  }

  bool operator==( const RasterList & other ) const
  {
    if ( collection_.get<RasterListByHashTag>().size() !=
         other.collection_.get<RasterListByHashTag>().size() ) {
      return false;
    }

    for ( auto const & item : collection_.get<RasterListByHashTag>() ) {
      if ( not other.has( item.hash ) ) {
        return false;
      }
    }

    return true;
  }

  bool operator!=( const RasterList & other ) const
  {
    return not ( ( *this ) == other );
  }
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

  // TODO(Deepak): Add methods here as needed
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
    hashed_unique
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
  search_by_track_id( const size_t track_id )
  {
    TrackDBCollectionByIds & index = collection_.get<TrackDBByIdsTag>();
    // Only specify the first field, since our ordered_index is sorted
    // first by track ID and then by frame ID
    return index.equal_range( track_id );
  }

  Optional<TrackData>
  search_next_frame( const size_t track_id, const SourceHash source_hash, const TargetHash target_hash )
  {
    TrackDBCollectionByFrameAndTrackId & hashed_index = collection_.get<TrackDBByFrameAndTrackIdTag>();
    boost::tuple<size_t, SourceHash, TargetHash> hashed_key =
      boost::make_tuple( track_id, source_hash, target_hash );
    if ( hashed_index.count( hashed_key ) == 0 )
      return Optional<TrackData>();
    TrackDBCollectionByFrameAndTrackId::iterator frame_data = hashed_index.find(
      hashed_key );
    // Find the frame_id of associated frame
    size_t frame_id = frame_data->frame_id;

    TrackDBCollectionByIds & ordered_index = collection_.get<TrackDBByIdsTag>();
    boost::tuple<size_t, size_t> ordered_key =
      boost::make_tuple( track_id, frame_id+1 );
    if ( ordered_index.count( ordered_key ) == 0)
      return Optional<TrackData>();
    TrackDBCollectionByIds::iterator ids_iterator = ordered_index.find(
      ordered_key);
    return make_optional( true, *ids_iterator );
  }

  void merge( const TrackDB & db )
  {
    TrackDBCollectionByIds & track_ids = collection_.get<TrackDBByIdsTag>();
    map<size_t, size_t> new_track_ids;

    for ( auto item : db.collection_.get<TrackDBSequencedTag>() ) {
      if ( new_track_ids.count( item.track_id ) > 0 ) {
        item.track_id = new_track_ids[ item.track_id ];
      }
      else if ( track_ids.count( item.track_id ) > 0 ) {
        size_t new_track_id = item.track_id;
        while ( track_ids.count ( ++new_track_id ) > 0 );
        new_track_ids[ item.track_id ] = new_track_id;
        item.track_id = new_track_id;
      }

      insert( item );
    }
  }
};

#endif /* MANIFESTS_HH */
