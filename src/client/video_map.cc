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
    track_lengths_ ( get_track_lengths( video_ ) )
{
  for ( unsigned int i = 0; i < tracks_.size(); i++ ) {
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
    lock.unlock();
  }

  cerr << "all done with track " << track_id << endl;
  
  /* confirm all finished okay */
  lock.lock();
  if ( tracks_.at( track_id ).size() != tracklen ) {
    throw runtime_error( "on track " + to_string( track_id ) + ", got "
			 + to_string( tracks_.at( track_id ).size() )
			 + " frames, expected " + to_string( tracklen ) );
  }

  RPC( "ClientReader::Finish", track_infos->Finish() );
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

vector<AnnotatedFrameInfo> VideoMap::track_snapshot( const unsigned int track_id,
						     const unsigned int start_frame_index ) const
{
  vector<AnnotatedFrameInfo> ret;
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

