#include "manifests.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

#define NUM_ELEMENTS 1000

/* Creates a mock track db with synthetic data. Mainly checks the index
   computation logic within the track db insert method. */
int main()
{
  TrackDB tdb;

  for (size_t i = 0; i < NUM_ELEMENTS; i++) {
    size_t track_id = i / 10;
    size_t frame_index = i % 10;
    size_t frame_id = NUM_ELEMENTS - i;

    // Make sure track_data is not in track_db yet
    if ( tdb.get_end_frame_index( track_id ) != frame_index ) {
      return 1;
    }

    // Insert track data into track_db
    TrackData track_data( track_id, frame_id );
    tdb.insert( track_data, true );

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
