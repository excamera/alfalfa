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

// TODO(Deepak): Fix composite key here
typedef multi_index_container
<
  TrackData,
  indexed_by
  <
    hashed_non_unique<member<TrackData, size_t, &TrackData::id>>,
    /*hashed_non_unique
    <
      composite_key
      <
        TrackData,
        member<TrackData, SourceHash, &TrackData::source_hash>,
        member<TrackData, TargetHash, &TrackData::target_hash>
      >
    >,*/
    sequenced<tag<TrackDBCollectionSequencedTag>>
  >
> TrackDBCollection;

typedef TrackDBCollection::nth_index<0>::type TrackDBCollectionById;
// typedef TrackDBCollection::nth_index<1>::type TrackDBCollectionByFrame;

class TrackDB : public BasicDatabase<TrackData, AlfalfaProtobufs::TrackData,
  TrackDBCollection, TrackDBCollectionSequencedTag>
{
public:
  TrackDB( const std::string & filename, const std::string & magic_number, OpenMode mode = OpenMode::READ )
  : BasicDatabase<TrackData, AlfalfaProtobufs::TrackData,
    TrackDBCollection, TrackDBCollectionSequencedTag>( filename, magic_number, mode )
  {}

  // TODO(Deepak): Add methods here as needed
};

#endif /* MANIFESTS_HH */
