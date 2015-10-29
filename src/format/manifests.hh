#ifndef MANIFESTS_HH
#define MANIFESTS_HH

#include <set>
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

struct RasterListCollectionSequencedTag;

typedef multi_index_container
<
  RasterData,
  indexed_by
  <
    hashed_unique<member<RasterData, size_t, &RasterData::hash>>,
    sequenced<tag<RasterListCollectionSequencedTag>>
  >
> RasterListCollection;

typedef RasterListCollection::nth_index<0>::type RasterListCollectionByHash;

class RasterList : public BasicDatabase<RasterData, AlfalfaProtobufs::RasterData,
  RasterListCollection, RasterListCollectionSequencedTag>
{
public:
  RasterList( const std::string & filename, const std::string & magic_number, OpenMode mode = OpenMode::READ )
  : BasicDatabase<RasterData, AlfalfaProtobufs::RasterData,
    RasterListCollection, RasterListCollectionSequencedTag>( filename, magic_number, mode )
  {}

  bool has( const size_t & raster_hash )
  {
    RasterListCollectionByHash & data_by_hash = collection_.get<0>();
    return data_by_hash.count( raster_hash ) > 0;
  }
};

/*
 * Quality DB
 *   sequence of original raster / approximate raster / quality
 */

struct QualityDBCollectionSequencedTag;

typedef multi_index_container
<
  QualityData,
  indexed_by
  <
    hashed_non_unique<member<QualityData, size_t, &QualityData::original_raster> >,
    hashed_non_unique<member<QualityData, size_t, &QualityData::approximate_raster> >,
    sequenced<tag<QualityDBCollectionSequencedTag> >
  >
> QualityDBCollection;

typedef QualityDBCollection::nth_index<0>::type QualityDBCollectionByOriginalRaster;
typedef QualityDBCollection::nth_index<1>::type QualityDBCollectionByApproximateRaster;

class QualityDB : public BasicDatabase<QualityData, AlfalfaProtobufs::QualityData,
  QualityDBCollection, QualityDBCollectionSequencedTag>
{
public:
  QualityDB( const std::string & filename, const std::string & magic_number, OpenMode mode = OpenMode::READ )
  : BasicDatabase<QualityData, AlfalfaProtobufs::QualityData,
    QualityDBCollection, QualityDBCollectionSequencedTag>( filename, magic_number, mode )
  {}

  // TODO(Deepak): Add methods here as needed
};

/*
 * Track DB
 *   mapping from ID to { sequence of frames }
 */

struct TrackDBCollectionSequencedTag;

typedef multi_index_container
<
  TrackData,
  indexed_by
  <
    ordered_unique
    <
      composite_key
      <
        TrackData,
        member<TrackData, size_t, &TrackData::track_id>,
        member<TrackData, size_t, &TrackData::frame_id>
      >
    >,
    hashed_unique
    <
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
    sequenced<tag<TrackDBCollectionSequencedTag>>
  >
> TrackDBCollection;

typedef TrackDBCollection::nth_index<0>::type TrackDBOrderedByIds;
typedef TrackDBCollection::nth_index<1>::type TrackDBHashedByFrameAndTrackId;

class TrackDB : public BasicDatabase<TrackData, AlfalfaProtobufs::TrackData,
  TrackDBCollection, TrackDBCollectionSequencedTag>
{
public:
  TrackDB( const std::string & filename, const std::string & magic_number, OpenMode mode = OpenMode::READ )
  : BasicDatabase<TrackData, AlfalfaProtobufs::TrackData,
    TrackDBCollection, TrackDBCollectionSequencedTag>( filename, magic_number, mode )
  {}

  std::pair<TrackDBOrderedByIds::iterator, TrackDBOrderedByIds::iterator>
  search_by_track_id( const size_t track_id )
  {
    TrackDBOrderedByIds & index = collection_.get<0>();
    // Only specify the first field, since our ordered_index is sorted
    // first by track ID and then by frame ID
    return index.equal_range( track_id );
  }

  Optional<TrackData>
  search_next_frame( const size_t track_id, const SourceHash source_hash, const TargetHash target_hash )
  {
    TrackDBHashedByFrameAndTrackId & hashed_index = collection_.get<1>();
    boost::tuple<size_t, SourceHash, TargetHash> hashed_key =
      boost::make_tuple( track_id, source_hash, target_hash );
    if ( hashed_index.count( hashed_key ) == 0 )
      return Optional<TrackData>();
    TrackDBHashedByFrameAndTrackId::iterator frame_data = hashed_index.find(
      hashed_key );
    // Find the frame_id of associated frame
    size_t frame_id = frame_data->frame_id;

    TrackDBOrderedByIds & ordered_index = collection_.get<0>();
    boost::tuple<size_t, size_t> ordered_key =
      boost::make_tuple( track_id, frame_id+1 );
    if ( ordered_index.count( ordered_key ) == 0)
      return Optional<TrackData>();
    TrackDBOrderedByIds::iterator ids_iterator = ordered_index.find(
      ordered_key);
    return make_optional( true, *ids_iterator );
  }
};

#endif /* MANIFESTS_HH */
