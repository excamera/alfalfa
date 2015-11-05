#include "manifests.hh"

VideoManifest::VideoManifest( const std::string & filename, const std::string & magic_number,
  OpenMode mode )
    : SerializableData( filename, magic_number, mode ), info_()
{
  if ( good_ == true and ( mode == OpenMode::READ or mode == OpenMode::APPEND ) ) {
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

bool VideoManifest::deserialize( const std::string & filename )
{
  std::string target_filename = ( filename.length() == 0 ) ? filename_ : filename;
  ProtobufDeserializer deserializer( target_filename );
  char target_magic_number[ MAX_MAGIC_NUMBER_LENGTH ] = {0};
  deserializer.read_raw( target_magic_number, magic_number.length() );

  if ( magic_number != target_magic_number ) {
    return false;
  }

  AlfalfaProtobufs::VideoInfo message;
  deserializer.read_protobuf( message );
  from_protobuf( message, info_ );
  deserializer.close();
  return true;
}

bool VideoManifest::serialize( const std::string & filename )
{
  if ( mode_ == OpenMode::READ ) {
    return false;
  }

  std::string target_filename = ( filename.length() == 0 ) ? filename_ : filename;
  filename_ = target_filename;
  ProtobufSerializer serializer( filename_ );

  // Writing the header
  if ( not serializer.write_raw( magic_number.c_str(), magic_number.length() ) ) {
    return false;
  }

  AlfalfaProtobufs::VideoInfo message;
  to_protobuf( info_, message );
  bool result = serializer.write_protobuf( message );
  serializer.close();
  return result;
}

bool RasterList::has( const size_t & raster_hash ) const
{
  const RasterListCollectionByHash & data_by_hash = collection_.get<RasterListByHashTag>();
  return data_by_hash.count( raster_hash ) > 0;
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

std::pair<QualityDBCollectionByOriginalRaster::iterator, QualityDBCollectionByOriginalRaster::iterator>
QualityDB::search_by_original_raster( const size_t & original_raster )
{
  QualityDBCollectionByOriginalRaster & index = collection_.get<QualityDBByOriginalRasterTag>();
  return index.equal_range( original_raster );
}

std::pair<QualityDBCollectionByApproximateRaster::iterator, QualityDBCollectionByApproximateRaster::iterator>
QualityDB::search_by_approximate_raster( const size_t & approximate_raster )
{
  QualityDBCollectionByApproximateRaster & index = collection_.get<QualityDBByApproximateRasterTag>();
  return index.equal_range( approximate_raster );
}

std::pair<TrackDBCollectionByIds::iterator, TrackDBCollectionByIds::iterator>
TrackDB::search_by_track_id( const size_t track_id )
{
  TrackDBCollectionByIds & index = collection_.get<TrackDBByIdsTag>();
  // Only specify the first field, since our ordered_index is sorted
  // first by track ID and then by frame ID
  return index.equal_range( track_id );
}

Optional<TrackData>
TrackDB::search_next_frame( const size_t track_id, const SourceHash source_hash, const TargetHash target_hash )
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

void TrackDB::merge( const TrackDB & db )
{
  TrackDBCollectionByIds & track_ids = collection_.get<TrackDBByIdsTag>();
  map<size_t, size_t> new_track_ids;

  for ( auto item : db.collection_.get<TrackDBSequencedTag>() ) {
    if ( new_track_ids.count( item.track_id ) > 0 ) {
      item.track_id = new_track_ids[ item.track_id ];
    }
    else if ( track_ids.count( item.track_id ) > 0 ) {
      size_t new_track_id = item.track_id;
      while ( track_ids.count ( new_track_id ) > 0 ) {
        new_track_id++;
      }
      new_track_ids[ item.track_id ] = new_track_id;
      item.track_id = new_track_id;
    } else {
      new_track_ids[item.track_id] = item.track_id;
    }
    insert( item );
  }
}
