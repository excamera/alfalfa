#include <algorithm>
#include <limits>

#include "video_map.hh"

using namespace std;

unsigned int find_max( vector<size_t> track_ids )
{
  if ( track_ids.empty() ) {
    throw runtime_error( "video has no tracks" );
  }

  sort( track_ids.begin(), track_ids.end() );

  for ( unsigned int i = 0; i < track_ids.size(); i++ ) {
    if ( i != track_ids[ i ] ) {
      throw runtime_error( "video does not have contiguous track_ids starting with 0" );
    }
  }
  
  return track_ids.back();
}

VideoMap::VideoMap( const string & server_address )
  : video_( server_address ),
    max_track_id_( find_max( video_.get_track_ids() ) ),
    fetcher_thread_( [&] () { fetch_all_tracks(); } )
{
  fetcher_thread_.detach();
}

void VideoMap::fetch_all_tracks()
{
  /* get the lengths */
  for ( unsigned int i = 0; i < max_track_id_; i++ ) {
    track_lengths_.push_back( video_.get_track_size( i ) );
  }

  const auto tracks_still_to_fetch = [&] () {
    for ( unsigned int i = 0; i < max_track_id_; i++ ) {
      if ( track_lengths_.at( i ) != tracks_.at( i ).size() ) {
	return true;
      }
    }
    return false;
  };
  
  /* start fetching the track details */
  while ( tracks_still_to_fetch() ) {
    for ( unsigned int i = 0; i < max_track_id_; i++ ) {
      if ( tracks_.at( i ).size() == track_lengths_.at( i ) ) {
	break;
      }
      
      const auto track = video_.get_abridged_frames( i,
						     tracks_.at( i ).size(),
						     min( tracks_.at( i ).size() + 240,
							  track_lengths_.at( i ) ) );
      for ( const auto & x : track.frame() ) {
	tracks_.at( i ).push_back( x );
      }
    }
  }

  cerr << "Got all the tracks.\n";
}
