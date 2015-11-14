#include "manifests.hh"

using namespace std;

VideoManifest::VideoManifest( const string & filename, const string & magic_number,
  OpenMode mode )
    : SerializableData( filename, magic_number, mode ), info_()
{
  if ( good_ == true and ( mode == OpenMode::READ ) ) {
    good_ = deserialize();
  }
}

void VideoManifest::set_info( const VideoInfo & info )
{
  info_.fourcc = info.fourcc;
  info_.width = info.width;
  info_.height = info.height;
  info_.frame_rate_numerator = info.frame_rate_numerator;
  info_.frame_rate_denominator = info.frame_rate_denominator;
  info_.frame_count = info.frame_count;
}

bool VideoManifest::deserialize()
{
  ProtobufDeserializer deserializer( filename_ );

  if ( magic_number != deserializer.read_string( magic_number.length() ) ) {
    return false;
  }

  AlfalfaProtobufs::VideoInfo message;
  deserializer.read_protobuf( message );
  info_ = from_protobuf( message );
  return true;
}

bool VideoManifest::serialize() const
{
  if ( mode_ == OpenMode::READ ) {
    return false;
  }

  ProtobufSerializer serializer( filename_ );

  // Writing the header
  serializer.write_string( magic_number );

  AlfalfaProtobufs::VideoInfo message = to_protobuf( info_ );
  return serializer.write_protobuf( message );
}

bool RasterList::has( const size_t & raster_hash ) const
{
  const RasterListCollectionByHash & data_by_hash = collection_.get<RasterListByHashTag>();
  return data_by_hash.count( raster_hash ) > 0;
}

size_t RasterList::raster( size_t raster_index )
{
  const RasterListCollectionRandomAccess & data_by_random_access = collection_.get<RasterListRandomAccessTag>();
  if ( data_by_random_access.size() <= raster_index )
    throw out_of_range( "Invalid raster index!" );
  return data_by_random_access.at( raster_index ).hash;
}

bool RasterList::operator==( const RasterList & other ) const
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

bool RasterList::operator!=( const RasterList & other ) const
{
  return not ( ( *this ) == other );
}

QualityData
QualityDB::search_by_original_and_approximate_raster( const size_t & original_raster, const size_t & approximate_raster )
{
  QualityDBCollectionByOriginalAndApproximateRaster & index =
    collection_.get<QualityDBByOriginalAndApproximateRasterTag>();
  auto key = boost::make_tuple( original_raster, approximate_raster );
  if ( index.count( key ) == 0 )
    throw out_of_range( "Raster pair not found!" );
  QualityDBCollectionByOriginalAndApproximateRaster::iterator it =
    index.find( key );
  return *it;
}

pair<QualityDBCollectionByOriginalRaster::iterator, QualityDBCollectionByOriginalRaster::iterator>
QualityDB::search_by_original_raster( const size_t & original_raster )
{
  QualityDBCollectionByOriginalRaster & index = collection_.get<QualityDBByOriginalRasterTag>();
  return index.equal_range( original_raster );
}

pair<QualityDBCollectionByApproximateRaster::iterator, QualityDBCollectionByApproximateRaster::iterator>
QualityDB::search_by_approximate_raster( const size_t & approximate_raster )
{
  QualityDBCollectionByApproximateRaster & index = collection_.get<QualityDBByApproximateRasterTag>();
  return index.equal_range( approximate_raster );
}

pair<TrackDBCollectionByTrackIdAndFrameIndex::iterator, TrackDBCollectionByTrackIdAndFrameIndex::iterator>
TrackDB::search_by_track_id( const size_t & track_id )
{
  TrackDBCollectionByTrackIdAndFrameIndex & index = collection_.get<TrackDBByTrackIdAndFrameIndexTag>();
  // Only specify the first field, since our ordered_index is sorted
  // first by track ID and then by frame ID
  return index.equal_range( track_id );
}

std::unordered_set<size_t>*
TrackDB::track_ids()
{
  std::unordered_set<size_t> *track_ids = new std::unordered_set<size_t>();
  TrackDBCollectionByTrackIdAndFrameIndex & index = collection_.get<TrackDBByTrackIdAndFrameIndexTag>();
  TrackDBCollectionByTrackIdAndFrameIndex::iterator beg = index.begin();
  TrackDBCollectionByTrackIdAndFrameIndex::iterator end = index.end();
  while(beg != end)
  {
    TrackData track_data = *beg;
    (*track_ids).insert( track_data.track_id );
    beg++;
  }
  return track_ids;
}


bool TrackDB::has_track( const size_t & track_id ) const
{
  const TrackDBCollectionByTrackIdAndFrameIndex & index = collection_.get<TrackDBByTrackIdAndFrameIndexTag>();
  return index.count( track_id ) > 0;
}

size_t
TrackDB::get_end_frame_index( const size_t & track_id ) const
{
  const TrackDBCollectionByTrackIdAndFrameIndex & index = collection_.get<TrackDBByTrackIdAndFrameIndexTag>();
  return index.count( track_id );
}

const TrackData &
TrackDB::get_frame( const size_t & track_id, const size_t & frame_index )
{
  TrackDBCollectionByTrackIdAndFrameIndex & ordered_index = collection_.get<TrackDBByTrackIdAndFrameIndexTag>();
  boost::tuple<size_t, size_t> ordered_key =
    boost::make_tuple( track_id, frame_index );
  if ( ordered_index.count( ordered_key ) == 0 )
    throw out_of_range( "Invalid (track_id, frame_index) pair" );
  TrackDBCollectionByTrackIdAndFrameIndex::iterator ids_iterator = ordered_index.find(
    ordered_key);
  return *ids_iterator;
}

void TrackDB::merge( const TrackDB & db, map<size_t, size_t> & frame_id_mapping )
{
  TrackDBCollectionByTrackIdAndFrameIndex & track_db_by_ids =
    collection_.get<TrackDBByTrackIdAndFrameIndexTag>();
  map<size_t, size_t> track_id_mapping;

  for ( auto item : db.collection_.get<TrackDBSequencedTag>() ) {
    if ( track_id_mapping.count( item.track_id ) > 0 ) {
      item.track_id = track_id_mapping[ item.track_id ];
    }
    else if ( track_db_by_ids.count( item.track_id ) > 0 ) {
      size_t new_track_id = item.track_id;
      while ( track_db_by_ids.count ( new_track_id ) > 0 ) {
        new_track_id++;
      }
      track_id_mapping[ item.track_id ] = new_track_id;
      item.track_id = new_track_id;
    } else {
      track_id_mapping[item.track_id] = item.track_id;
    }
    item.frame_id = frame_id_mapping[item.frame_id];
    insert( item );
  }
}

TrackDBIterator &
TrackDBIterator::operator++()
{
  frame_index_++;
  return *this;
}

TrackDBIterator
TrackDBIterator::operator++( int )
{
  TrackDBIterator tmp( *this );
  ++( *this );
  return tmp;
}

bool
TrackDBIterator::operator==( const TrackDBIterator & rhs ) const
{
  return (track_id_ == rhs.track_id()) and (frame_index_ == rhs.frame_index());
}

bool
TrackDBIterator::operator!=( const TrackDBIterator & rhs ) const
{
  return !((track_id_ == rhs.track_id()) and (frame_index_ == rhs.frame_index()));
}

const FrameInfo &
TrackDBIterator::operator*() const
{
  const TrackData & track_data = track_db_.get_frame( track_id_, frame_index_ );
  const size_t & frame_id = track_data.frame_id;
  return frame_db_.search_by_frame_id( frame_id );
}

const FrameInfo *
TrackDBIterator::operator->() const
{
  const TrackData & track_data = track_db_.get_frame( track_id_, frame_index_ );
  const size_t & frame_id = track_data.frame_id;
  return &frame_db_.search_by_frame_id( frame_id );
}

bool
SwitchDB::has_switch( const size_t & from_track_id, const size_t & to_track_id,
                      const size_t & from_frame_index )
{
  SwitchDBCollectionHashedByTrackIdsAndFrameIndex & index = collection_.get<SwitchDBHashedByTrackIdsAndFrameIndexTag>();
  return index.count( boost::make_tuple( from_track_id, to_track_id, from_frame_index ) ) > 0;
}

size_t
SwitchDB::get_end_switch_frame_index( const size_t & from_track_id, const size_t & to_track_id,
                               const size_t & from_frame_index )
{
  SwitchDBCollectionHashedByTrackIdsAndFrameIndex & index = collection_.get<SwitchDBHashedByTrackIdsAndFrameIndexTag>();
  return index.count( boost::make_tuple( from_track_id, to_track_id, from_frame_index ) );
}

size_t
SwitchDB::get_to_frame_index( const size_t & from_track_id, const size_t & to_track_id,
                    const size_t & from_frame_index )
{
  SwitchDBCollectionHashedByTrackIdsAndFrameIndex & index = collection_.get<SwitchDBHashedByTrackIdsAndFrameIndexTag>();
  return index.find( boost::make_tuple( from_track_id, to_track_id, from_frame_index ) )->to_frame_index;
}


const SwitchData &
SwitchDB::get_frame( const size_t & from_track_id, const size_t & to_track_id,
                     const size_t & from_frame_index, const size_t & switch_frame_index )
{
  SwitchDBCollectionOrderedByTrackIdsAndFrameIndices & ordered_index = collection_.get<SwitchDBOrderedByTrackIdsAndFrameIndicesTag>();
  boost::tuple<size_t, size_t, size_t, size_t> ordered_key =
    boost::make_tuple( from_track_id, to_track_id, from_frame_index, switch_frame_index );
  if ( ordered_index.count( ordered_key ) == 0 )
    throw out_of_range( "Frame not found in switch DB" );
  SwitchDBCollectionOrderedByTrackIdsAndFrameIndices::iterator ids_iterator = ordered_index.find(
    ordered_key);
  return *ids_iterator;
}

SwitchDBIterator &
SwitchDBIterator::operator++()
{
  switch_frame_index_++;
  return *this;
}

SwitchDBIterator
SwitchDBIterator::operator++( int )
{
  SwitchDBIterator tmp( *this );
  ++( *this );
  return tmp;
}

bool
SwitchDBIterator::operator==( const SwitchDBIterator & rhs ) const
{
  return (from_track_id_ == rhs.from_track_id()) and
         (to_track_id_ == rhs.to_track_id()) and
         (switch_frame_index_ == rhs.switch_frame_index()) and
         (from_frame_index_ == rhs.from_frame_index());
}

bool
SwitchDBIterator::operator!=( const SwitchDBIterator & rhs ) const
{
  return !((from_track_id_ == rhs.from_track_id()) and
          (to_track_id_ == rhs.to_track_id()) and
          (switch_frame_index_ == rhs.switch_frame_index()) and
          (from_frame_index_ == rhs.from_frame_index()));
}

const FrameInfo &
SwitchDBIterator::operator*() const
{
  const SwitchData & switch_data = switch_db_.get_frame( from_track_id_, to_track_id_,
                                                         from_frame_index_,
                                                         switch_frame_index_ );
  const size_t & frame_id = switch_data.frame_id;
  return frame_db_.search_by_frame_id( frame_id );
}

const FrameInfo *
SwitchDBIterator::operator->() const
{
  const SwitchData & switch_data = switch_db_.get_frame( from_track_id_, to_track_id_,
                                                         from_frame_index_,
                                                         switch_frame_index_ );
  const size_t & frame_id = switch_data.frame_id;
  return &frame_db_.search_by_frame_id( frame_id );
}
