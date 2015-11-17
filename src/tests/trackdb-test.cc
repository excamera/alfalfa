#include "manifests.hh"

using namespace std;

#define NUM_ELEMENTS 1000

/* Creates a mock track db with synthetic data. Mainly checks the index
   computation logic within the track db insert method. */
int main()
{
  std::string db = "test-track-db";

  TrackDB tdb( db, "ALFATRDB", OpenMode::TRUNCATE );

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

    size_t track_id = i / 10;
    size_t frame_index = i % 10;
    size_t frame_id = NUM_ELEMENTS - i;

    // Make sure track_data is not in track_db yet
    if ( tdb.get_end_frame_index( track_id ) != frame_index ) {
      return 1;
    }

    // Insert track data into track_db
    TrackData track_data( track_id, frame_id );
    tdb.insert( track_data );

    // Make sure new track_data is reflected in the indices
    if ( tdb.get_end_frame_index( track_id) != (frame_index + 1) ) {
      return 1;
    }
    TrackData track_data_returned = tdb.get_frame( track_id, frame_index );
    if ( track_data_returned.frame_id != frame_id || track_data_returned.track_id != track_id ) {
      return 1;
    }
  }

  return 0;
}
