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
}

bool VideoManifest::deserialize()
{
  ProtobufDeserializer deserializer( filename_ );

  if ( magic_number != deserializer.read_string( magic_number.length() ) ) {
    return false;
  }

  AlfalfaProtobufs::VideoInfo message;
  deserializer.read_protobuf( message );
  info_ = VideoInfo( message );
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

  AlfalfaProtobufs::VideoInfo message = info_.to_protobuf();
  return serializer.write_protobuf( message );
}

bool RasterList::has( const size_t raster_hash ) const
{
  const RasterListCollectionByHash & data_by_hash = collection_.get<RasterListByHashTag>();
  return data_by_hash.count( raster_hash ) > 0;
}

RasterData RasterList::raster( const size_t raster_index ) const
{
  const RasterListCollectionRandomAccess & data_by_random_access = collection_.get<RasterListRandomAccessTag>();
  if ( data_by_random_access.size() <= raster_index )
    throw out_of_range( "Invalid raster index!" );
  return data_by_random_access.at( raster_index );
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
QualityDB::search_by_original_and_approximate_raster( const size_t original_raster,
                                                      const size_t approximate_raster ) const
{
  const QualityDBCollectionByOriginalAndApproximateRaster & index =
    collection_.get<QualityDBByOriginalAndApproximateRasterTag>();
  auto key = boost::make_tuple( original_raster, approximate_raster );
  if ( index.count( key ) == 0 ) {
    throw out_of_range( "Raster pair not found!" );
  }
  QualityDBCollectionByOriginalAndApproximateRaster::const_iterator it =
    index.find( key );
  return *it;
}

pair<QualityDBCollectionByOriginalRaster::const_iterator, QualityDBCollectionByOriginalRaster::const_iterator>
QualityDB::search_by_original_raster( const size_t original_raster ) const
{
  const QualityDBCollectionByOriginalRaster & index = collection_.get<QualityDBByOriginalRasterTag>();
  return index.equal_range( original_raster );
}

pair<QualityDBCollectionByApproximateRaster::const_iterator, QualityDBCollectionByApproximateRaster::const_iterator>
QualityDB::search_by_approximate_raster( const size_t approximate_raster ) const
{
  const QualityDBCollectionByApproximateRaster & index = collection_.get<QualityDBByApproximateRasterTag>();
  return index.equal_range( approximate_raster );
}

size_t TrackDB::insert( TrackData td )
{
  track_ids_.insert( td.track_id );
  size_t frame_index;
  if ( track_frame_indices_.count( td.track_id ) == 0 ) {
    frame_index = 0;
  } else {
    frame_index = track_frame_indices_[ td.track_id ];
  }
  track_frame_indices_[ td.track_id ] = frame_index + 1;
  td.frame_index = frame_index;
  collection_.insert( td );
  return frame_index;
}

pair<unordered_set<size_t>::const_iterator, unordered_set<size_t>::const_iterator>
TrackDB::get_track_ids() const
{
  return make_pair( track_ids_.begin(), track_ids_.end() );
}

bool TrackDB::has_track( const size_t track_id ) const
{
  const TrackDBCollectionByTrackIdAndFrameIndex & index = collection_.get<TrackDBByTrackIdAndFrameIndexTag>();
  return index.count( track_id ) > 0;
}

size_t
TrackDB::get_end_frame_index( const size_t track_id ) const
{
  const TrackDBCollectionByTrackIdAndFrameIndex & index = collection_.get<TrackDBByTrackIdAndFrameIndexTag>();
  return index.count( track_id );
}

const TrackData &
TrackDB::get_frame( const size_t & track_id, const size_t & frame_index ) const
{
  const TrackDBCollectionByTrackIdAndFrameIndex & ordered_index = collection_.get<TrackDBByTrackIdAndFrameIndexTag>();
  boost::tuple<size_t, size_t> ordered_key =
    boost::make_tuple( track_id, frame_index );
  if ( ordered_index.count( ordered_key ) == 0 )
    throw out_of_range( "Invalid (track_id, frame_index) pair" );
  TrackDBCollectionByTrackIdAndFrameIndex::const_iterator ids_iterator = ordered_index.find(
    ordered_key);
  return *ids_iterator;
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
SwitchDB::has_switch( const size_t from_track_id, const size_t to_track_id,
                      const size_t from_frame_index ) const
{
  const SwitchDBCollectionHashedByTrackIdsAndFrameIndex & index = collection_.get<SwitchDBHashedByTrackIdsAndFrameIndexTag>();
  return index.count( boost::make_tuple( from_track_id, to_track_id, from_frame_index ) ) > 0;
}

size_t
SwitchDB::get_end_switch_frame_index( const size_t from_track_id, const size_t to_track_id,
                                      const size_t from_frame_index ) const
{
  const SwitchDBCollectionHashedByTrackIdsAndFrameIndex & index = collection_.get<SwitchDBHashedByTrackIdsAndFrameIndexTag>();
  return index.count( boost::make_tuple( from_track_id, to_track_id, from_frame_index ) );
}

size_t
SwitchDB::get_to_frame_index( const size_t from_track_id, const size_t to_track_id,
                              const size_t from_frame_index ) const
{
  const SwitchDBCollectionHashedByTrackIdsAndFrameIndex & index = collection_.get<SwitchDBHashedByTrackIdsAndFrameIndexTag>();
  return index.find( boost::make_tuple( from_track_id, to_track_id, from_frame_index ) )->to_frame_index;
}

const SwitchData &
SwitchDB::get_frame( const size_t from_track_id, const size_t to_track_id,
                     const size_t from_frame_index, const size_t switch_frame_index ) const
{
  const SwitchDBCollectionOrderedByTrackIdsAndFrameIndices & ordered_index = collection_.get<SwitchDBOrderedByTrackIdsAndFrameIndicesTag>();
  boost::tuple<size_t, size_t, size_t, size_t> ordered_key =
    boost::make_tuple( from_track_id, to_track_id, from_frame_index, switch_frame_index );
  if ( ordered_index.count( ordered_key ) == 0 )
    throw out_of_range( "Frame not found in switch DB" );
  SwitchDBCollectionOrderedByTrackIdsAndFrameIndices::const_iterator ids_iterator = ordered_index.find(
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
