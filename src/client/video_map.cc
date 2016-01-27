#include <algorithm>
#include <limits>

#include "video_map.hh"

using namespace std;
using namespace grpc;

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
    tracks_(),
    shown_frame_counts_()
{
  for ( unsigned int i = 0; i < track_lengths_.size(); i++ ) {
    tracks_.emplace_back();
    shown_frame_counts_.emplace_back();
    fetchers_.emplace_back( [&] ( unsigned int track_id ) { fetch_track( track_id ); }, i );
    fetchers_.back().detach();
  }
}

void VideoMap::fetch_track( unsigned int track_id )
{
  /* start fetching the track details */
  unique_lock<mutex> lock { mutex_ };
  grpc::ClientContext client_context;
  const unsigned int tracklen = track_lengths_.at( track_id );
  auto track_infos = video_.get_abridged_frames( client_context, track_id, 0, tracklen );

  lock.unlock();
  
  AlfalfaProtobufs::AbridgedFrameInfo frame;
  while ( track_infos->Read( &frame ) ) {
    lock.lock();
    tracks_.at( track_id ).emplace_back( frame );
    if ( frame.shown() ) {
      tracks_.at( track_id ).back().timestamp = shown_frame_counts_.at( track_id );
      shown_frame_counts_.at( track_id )++;
    }
    lock.unlock();
  }

  cerr << "all done with track " << track_id << endl;
  
  /* confirm all finished okay */
  lock.lock();
  RPC( "ClientReader::Finish", track_infos->Finish() );
  if ( tracks_.at( track_id ).size() != tracklen ) {
    throw runtime_error( "on track " + to_string( track_id ) + ", got "
			 + to_string( tracks_.at( track_id ).size() )
			 + " frames, expected " + to_string( tracklen ) );
  }
}

unsigned int VideoMap::track_length_full( const unsigned int track_id ) const
{
  return track_lengths_.at( track_id );
}

unsigned int VideoMap::track_length_now( const unsigned int track_id ) const
{
  unique_lock<mutex> lock { mutex_ };
  return tracks_.at( track_id ).size();
}

deque<AnnotatedFrameInfo> VideoMap::track_snapshot( const unsigned int track_id,
						     const unsigned int start_frame_index ) const
{
  deque<AnnotatedFrameInfo> ret;
  unique_lock<mutex> lock { mutex_ };
  const auto & track = tracks_.at( track_id );
  if ( start_frame_index > track.size() ) {
    throw out_of_range( "attempt to read beyond end of track" );
  }
				       
  ret.insert( ret.begin(),
	      track.begin() + start_frame_index,
	      track.end() );
  return ret;
}

AnnotatedFrameInfo::AnnotatedFrameInfo( const AlfalfaProtobufs::AbridgedFrameInfo & fi )
  : offset( fi.offset() ),
    length( fi.length() ),
    frame_id( fi.frame_id() ),
    key( fi.key() ),
    shown( fi.shown() ),
    quality( fi.quality() )
{}

AnnotatedFrameInfo::AnnotatedFrameInfo( const FrameInfo & fi )
  : offset( fi.offset() ),
    length( fi.length() ),
    frame_id( fi.frame_id() ),
    key( fi.is_keyframe() ),
    shown( fi.shown() ),
    quality()
{}

void VideoMap::update_annotations( const double estimated_bytes_per_second_,
				   const unordered_map<uint64_t, pair<uint64_t, size_t>> frame_store_ )
{
  thread newthread( [&] ( const double estimated_bytes_per_second, const unordered_map<uint64_t, pair<uint64_t, size_t>> frame_store ) { 
      std::unique_lock<mutex> locked { annotation_mutex_, try_to_lock };
      if ( not locked ) {
	cerr << "skipping redundant run of frame annotations\n";
	return;
      }

      unique_lock<mutex> lock { mutex_ };
    
      for ( auto & track : tracks_ ) {
	unsigned int shown_frame_count = 0;
	double running_mean = 0.0;
	double running_varsum = 0.0;
	uint64_t bytes_to_download_from_here = 0;
    
	/* algorithm from Knuth volume 2, per http://www.johndcook.com/blog/standard_deviation/ */
	for ( auto frame = track.rbegin(); frame != track.rend(); frame++ ) {
	  if ( frame->shown ) {
	    shown_frame_count++;
	  }

	  if ( shown_frame_count == 0 ) {
	    frame->average_quality_to_end = 1;
	    frame->stddev_quality_to_end = 0;
	  } else {
	    const double new_mean = running_mean + ( frame->quality - running_mean ) / shown_frame_count;
	    const double new_varsum = running_varsum + ( frame->quality - running_mean ) * ( frame->quality - new_mean );
	    tie( running_mean, running_varsum ) = make_tuple( new_mean, new_varsum );

	    frame->average_quality_to_end = running_mean;
	    frame->stddev_quality_to_end = sqrt( running_varsum / ( shown_frame_count - 1.0 ) );
	  }
      
	  frame->remaining_time_in_video = ( shown_frame_count - 1.0 ) / 24.0;

	  if ( frame_store.find( frame->offset ) == frame_store.end() ) {
	    bytes_to_download_from_here += frame->length;
	  }

	  frame->download_time_required_from_here = bytes_to_download_from_here / estimated_bytes_per_second;
	}
      }

      analysis_generation_++;
    }, estimated_bytes_per_second_, move( frame_store_ ) );

  newthread.detach();
}
