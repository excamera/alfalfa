#ifndef MANIFESTS_HH
#define MANIFESTS_HH

#include <set>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <unordered_set>

#include "db.hh"
#include "frame_db.hh"
#include "dependency_tracking.hh"
#include "serialization.hh"
#include "alfalfa.pb.h"

/*
 * VideoManifest
 */

class VideoManifest
{
private:
  VideoInfo info_;
  static VideoInfo deserialize( FileDescriptor && fd );

public:
  VideoManifest( FileDescriptor && fd );
  VideoManifest( const uint16_t width, const uint16_t height );
  
  const VideoInfo & info() const { return info_; }
  VideoInfo & mutable_info() { return info_; }

  void serialize( FileDescriptor && fd ) const;
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
    ordered_non_unique
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

struct RasterMagic { static constexpr auto magic = "ALFARSLS"; };

class RasterList : public BasicDatabase<RasterData, AlfalfaProtobufs::RasterData,
					RasterListCollection, RasterListSequencedTag,
					RasterMagic>
{
public:
  using BasicDatabase::BasicDatabase;

  bool has( const size_t raster_hash ) const;
  RasterData raster( const size_t raster_index ) const;
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
    hashed_non_unique
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
typedef QualityDBCollection::index<QualityDBSequencedTag>::type
QualityDBCollectionSequencedAccess;

struct QualityMagic { static constexpr auto magic = "ALFAQLDB"; };

class QualityDB : public BasicDatabase<QualityData, AlfalfaProtobufs::QualityData,
				       QualityDBCollection, QualityDBSequencedTag,
				       QualityMagic>
{
public:
  using BasicDatabase::BasicDatabase;

  QualityData
  search_by_original_and_approximate_raster( const size_t original_raster, const size_t approximate_raster ) const;
  std::pair<QualityDBCollectionByOriginalRaster::const_iterator, QualityDBCollectionByOriginalRaster::const_iterator>
  search_by_original_raster( const size_t original_raster ) const;
  std::pair<QualityDBCollectionByApproximateRaster::const_iterator, QualityDBCollectionByApproximateRaster::const_iterator>
  search_by_approximate_raster( const size_t approximate_raster ) const;
};

/*
 * Track DB
 *   mapping from ID to { sequence of frames }
 */

struct TrackDBByTrackIdAndFrameIndexTag;
struct TrackDBByFrameIdTag;
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
    hashed_non_unique
    <
      tag<TrackDBByFrameIdTag>,
      member<TrackData, size_t, &TrackData::frame_id>
    >,
    sequenced
    <
      tag<TrackDBSequencedTag>
    >
  >
> TrackDBCollection;

typedef TrackDBCollection::index<TrackDBByTrackIdAndFrameIndexTag>::type
TrackDBCollectionByTrackIdAndFrameIndex;
typedef TrackDBCollection::index<TrackDBByFrameIdTag>::type
TrackDBCollectionByFrameIdIndex;
typedef TrackDBCollection::index<TrackDBSequencedTag>::type
TrackDBCollectionSequencedAccess;

struct TrackMagic { static constexpr auto magic = "ALFATRDB"; };

class TrackDB : public BasicDatabase<TrackData, AlfalfaProtobufs::TrackData,
				     TrackDBCollection, TrackDBSequencedTag,
				     TrackMagic>
{
private:
  std::unordered_set<size_t> track_ids_ {};
  std::unordered_map<size_t, size_t> track_frame_indices_ {};
  std::unordered_map<size_t, size_t> track_displayed_raster_indices_ {};

public:
  TrackDB() {}
  
  TrackDB( FileDescriptor && fd )
    : BasicDatabase( std::move( fd ) )
  {
    for ( auto it = begin(); it != end(); it++ ) {
      track_ids_.insert( it->track_id );
      track_frame_indices_[ it->track_id ] = get_end_frame_index( it->track_id );
    }
  }

  size_t insert( TrackData td );
  size_t insert( TrackData td, bool shown );

  std::pair<std::unordered_set<size_t>::const_iterator, std::unordered_set<size_t>::const_iterator>
  get_track_ids() const;

  const TrackData &
  get_frame( const size_t & track_id, const size_t & frame_index ) const;

  std::pair<TrackDBCollectionByFrameIdIndex::const_iterator,
    TrackDBCollectionByFrameIdIndex::const_iterator>
  search_by_frame_id( const size_t frame_id ) const;
  size_t get_end_frame_index( const size_t track_id ) const;
  bool has_track( const size_t track_id ) const;
};

/*
 * TrackDB iterator
 *   iterator over FrameInfo objects -- useful while iterating through track DB
 */
class TrackDBIterator : std::iterator<std::forward_iterator_tag, FrameInfo>
{
  private:
    size_t track_id_;
    size_t frame_index_;
    const TrackDB & track_db_;
    const FrameDB & frame_db_;

  public:
    TrackDBIterator( size_t track_id, size_t start_frame_index,
                     const TrackDB & track_db,
                     const FrameDB & frame_db )
    : track_id_( track_id ), frame_index_( start_frame_index ), track_db_( track_db ),
      frame_db_( frame_db )
    {}

    size_t track_id() const { return track_id_; }
    size_t frame_index() const { return frame_index_; }

    TrackDBIterator & operator++();
    TrackDBIterator operator++( int );

    TrackDBIterator & operator--();
    TrackDBIterator operator--( int );

    bool operator==( const TrackDBIterator & rhs ) const;
    bool operator!=( const TrackDBIterator & rhs ) const;

    const FrameInfo & operator*() const;
    const FrameInfo * operator->() const;
};

/*
 * Switch DB
 *   stores ids of frames that help to switch between different tracks in the
 *   track DB
 */

struct SwitchDBSequencedTag;
struct SwitchDBHashedByTrackIdsAndFrameIndexTag;
struct SwitchDBOrderedByTrackIdsAndFrameIndicesTag;
struct SwitchDBHashedByFrameIdsTag;

typedef multi_index_container
<
  SwitchData,
  indexed_by
  <
    hashed_non_unique
    <
      tag<SwitchDBHashedByTrackIdsAndFrameIndexTag>,
      composite_key
      <
        SwitchData,
        member<SwitchData, size_t, &SwitchData::from_track_id>,
        member<SwitchData, size_t, &SwitchData::to_track_id>,
        member<SwitchData, size_t, &SwitchData::from_frame_index>
      >
    >,
    ordered_non_unique
    <
      tag<SwitchDBOrderedByTrackIdsAndFrameIndicesTag>,
      composite_key
      <
        SwitchData,
        member<SwitchData, size_t, &SwitchData::from_track_id>,
        member<SwitchData, size_t, &SwitchData::to_track_id>,
        member<SwitchData, size_t, &SwitchData::from_frame_index>,
        member<SwitchData, size_t, &SwitchData::switch_frame_index>
      >
    >,
    hashed_non_unique
    <
      tag<SwitchDBHashedByFrameIdsTag>,
      member<SwitchData, size_t, &SwitchData::frame_id>
    >,
    sequenced
    <
      tag<SwitchDBSequencedTag>
    >
  >
> SwitchDBCollection;

typedef SwitchDBCollection::index<SwitchDBHashedByTrackIdsAndFrameIndexTag>::type
SwitchDBCollectionHashedByTrackIdsAndFrameIndex;
typedef SwitchDBCollection::index<SwitchDBOrderedByTrackIdsAndFrameIndicesTag>::type
SwitchDBCollectionOrderedByTrackIdsAndFrameIndices;
typedef SwitchDBCollection::index<SwitchDBHashedByFrameIdsTag>::type
SwtichDBCollectionHashedByFrameIds;
typedef SwitchDBCollection::index<SwitchDBSequencedTag>::type
SwitchDBCollectionSequencedAccess;

struct SwitchMagic { static constexpr auto magic = "ALFASWDB"; };

class SwitchDB : public BasicDatabase<SwitchData, AlfalfaProtobufs::SwitchData,
				      SwitchDBCollection, SwitchDBSequencedTag,
				      SwitchMagic>
{
public:
  using BasicDatabase::BasicDatabase;

  size_t get_end_switch_frame_index( const size_t from_track_id,
                                     const size_t to_track_id,
                                     const size_t from_frame_index ) const;
  bool has_switch( const size_t from_track_id, const size_t to_track_id,
                   const size_t from_frame_index ) const;
  size_t get_to_frame_index( const size_t from_track_id,
                             const size_t to_track_id,
                             const size_t from_frame_index ) const;
  const SwitchData &
  get_frame( const size_t from_track_id, const size_t to_track_id,
             const size_t from_frame_index, const size_t switch_frame_index ) const;

  std::pair<SwtichDBCollectionHashedByFrameIds::const_iterator,
  SwtichDBCollectionHashedByFrameIds::const_iterator>
  get_switches_by_frame_id( const size_t frame_id ) const;
};

/*
 * SwitchDB iterator
 *   iterator over FrameInfo objects -- useful while iterating through switch DB
 */
class SwitchDBIterator : std::iterator<std::forward_iterator_tag, FrameInfo>
{
  private:
    size_t from_track_id_;
    size_t to_track_id_;
    size_t from_frame_index_;
    size_t switch_frame_index_;
    size_t to_frame_index_;
    const SwitchDB & switch_db_;
    const FrameDB & frame_db_;

  public:
    SwitchDBIterator( size_t from_track_id, size_t to_track_id, size_t from_frame_index,
                      size_t start_switch_frame_index, const SwitchDB & switch_db,
                      const FrameDB & frame_db )
    : from_track_id_( from_track_id ), to_track_id_( to_track_id ),
      from_frame_index_(from_frame_index), switch_frame_index_(start_switch_frame_index),
      to_frame_index_(0), switch_db_(switch_db), frame_db_(frame_db)
    {
      to_frame_index_ = switch_db_.get_to_frame_index( from_track_id_, to_track_id_,
                                                       from_frame_index_ );
    }

    const size_t & from_track_id() const { return from_track_id_; }
    const size_t & to_track_id() const { return to_track_id_; }
    const size_t & from_frame_index() const { return from_frame_index_; }
    const size_t & switch_frame_index() const { return switch_frame_index_; }
    const size_t & to_frame_index() const { return to_frame_index_; }

    SwitchDBIterator & operator++();
    SwitchDBIterator operator++( int );

    SwitchDBIterator & operator--();
    SwitchDBIterator operator--( int );

    bool operator==( const SwitchDBIterator & rhs ) const;
    bool operator!=( const SwitchDBIterator & rhs ) const;

    const FrameInfo & operator*() const;
    const FrameInfo * operator->() const;
};


#endif /* MANIFESTS_HH */
