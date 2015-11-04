#include "manifests.hh"

using namespace std;

#define NUM_ELEMENTS 100

int track_db_collection_test() {
  TrackDBCollection track_db_collection;
  TrackDBCollectionByIds &
    track_db_collection_ordered = track_db_collection.get<TrackDBByIdsTag>();
  TrackDBCollectionByFrameAndTrackId &
    track_db_collection_hashed = track_db_collection.get<TrackDBByFrameAndTrackIdTag>();
  for (size_t i = 0; i < NUM_ELEMENTS; i++) {
    SourceHash source_hash(
      make_optional( ( i >> 0 ) & 1, i * 1234 + 0 ),
      make_optional( ( i >> 1 ) & 1, i * 1234 + 1 ),
      make_optional( ( i >> 2 ) & 1, i * 1234 + 2 ),
      make_optional( ( i >> 3 ) & 1, i * 1234 + 3 ),
      make_optional( ( i >> 4 ) & 1, i * 1234 + 4 )
    );

    UpdateTracker update_tracker( ( i % 2 ) & 1, ( i % 3 ) & 1, ( i % 4 ) & 1 , ( i % 5 ) & 1,
      ( i % 6 ) & 1, ( i % 7 ) & 1, ( i % 8 ) & 1 );

    TargetHash target_hash( update_tracker, i * 1234 + 5, i * 1234 + 6,
      i * 1234 + 7, ( i % 5 ) &  1 );

    size_t frame_id = i % 10;
    size_t track_id = i / 10;

    // Make sure track_data is not in track_db yet
    if ( track_db_collection_hashed.count( boost::make_tuple(
      track_id, source_hash, target_hash ) ) > 0 ) {
      return 1;
    }

    // Insert track data into track_db
    TrackData track_data( track_id, frame_id, source_hash, target_hash );
    track_db_collection.insert( track_data );

    // Make sure new track_data is reflected in the indices
    if ( track_db_collection_hashed.count( boost::make_tuple(
      track_id, source_hash, target_hash ) ) == 0 ) {
      return 1;
    }
  }

  // Check if ordering guarantees are maintained
  for (size_t track_id = 0; track_id < 10; track_id++) {
    auto id_iterators = track_db_collection_ordered.equal_range( track_id );
    TrackDBCollectionByIds::iterator begin = id_iterators.first;
    TrackDBCollectionByIds::iterator end = id_iterators.second;
    size_t prev_frame_id = 0;
    while (begin != end) {
      if (begin->track_id != track_id)
        return 1;
      if (begin->frame_id < prev_frame_id)
        return 1;
      prev_frame_id = begin->frame_id;
      begin++;
    }
  }
  
  return 0;
}

int main()
{
  return track_db_collection_test();
}
