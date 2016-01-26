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

vector<size_t> get_track_lengths( const AlfalfaVideoClient & video )
{
  unsigned int max_track_id = find_max( video.get_track_ids() );
  vector<size_t> ret;
  
  for ( unsigned int i = 0; i <= max_track_id; i++ ) {
    ret.push_back( video.get_track_size( i ) );
  }

  return ret;
}

VideoMap::VideoMap( const string & server_address )
  : video_( server_address ),
    track_lengths_ ( get_track_lengths( video_ ) ),
    fetcher_thread_( [&] () { fetch_all_tracks(); } )
{
  fetcher_thread_.detach();
}

void VideoMap::fetch_all_tracks()
{
  unique_lock<mutex> lock { mutex_ };

  const auto tracks_still_to_fetch = [&] () {
    for ( unsigned int i = 0; i < tracks_.size(); i++ ) {
      if ( track_lengths_.at( i ) != tracks_.at( i ).size() ) {
	return true;
      }
    }
    return false;
  };
  
  /* start fetching the track details */
  while ( tracks_still_to_fetch() ) {
    for ( unsigned int i = 0; i < tracks_.size(); i++ ) {
      if ( tracks_.at( i ).size() == track_lengths_.at( i ) ) {
	break;
      }


      const unsigned int start_frame = tracks_.at( i ).size();
      const unsigned int stop_frame = min( tracks_.at( i ).size() + 1000,
					   track_lengths_.at( i ) );

      lock.unlock();
      const auto track = video_.get_abridged_frames( i, start_frame, stop_frame );

      lock.lock();
      for ( const auto & x : track.frame() ) {
	tracks_.at( i ).push_back( x );
      }

      cerr << "track " << i << " now has " << tracks_.at( i ).size() << " frames\n";
    }
  }

  cerr << "Got all the tracks.\n";
}
