#include <cstdio>
#include <sstream>
#include <algorithm>
#include <forward_list>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "ivf.hh"
#include "exception.hh"
#include "alfalfa_video.hh"
#include "tracking_player.hh"
#include "filesystem.hh"
#include "subprocess.hh"
#include "temp_file.hh"

using namespace std;

const string FRAME_DB_FILENAME = "frame.db";
const string RASTER_LIST_FILENAME = "raster.db";
const string QUALITY_DB_FILENAME = "quality.db";
const string TRACK_DB_FILENAME = "track.db";
const string SWITCH_DB_FILENAME = "switch.db";
const string VIDEO_MANIFEST_FILENAME = "video.manifest";
const string IVF_FILENAME = "v";
const string VPXENC_EXECUTABLE = "vpxenc";

AlfalfaVideo::VideoDirectory::VideoDirectory( const string & path )
  : directory_path_( path )
{}

string AlfalfaVideo::VideoDirectory::video_manifest_filename() const
{
  return FileSystem::append( directory_path_, VIDEO_MANIFEST_FILENAME );
}

string AlfalfaVideo::VideoDirectory::raster_list_filename() const
{
  return FileSystem::append( directory_path_, RASTER_LIST_FILENAME );
}

string AlfalfaVideo::VideoDirectory::quality_db_filename() const
{
  return FileSystem::append( directory_path_, QUALITY_DB_FILENAME );
}

string AlfalfaVideo::VideoDirectory::frame_db_filename() const
{
  return FileSystem::append( directory_path_, FRAME_DB_FILENAME );
}

string AlfalfaVideo::VideoDirectory::track_db_filename() const
{
  return FileSystem::append( directory_path_, TRACK_DB_FILENAME );
}

string AlfalfaVideo::VideoDirectory::switch_db_filename() const
{
  return FileSystem::append( directory_path_, SWITCH_DB_FILENAME );
}

string AlfalfaVideo::VideoDirectory::ivf_filename() const
{
  return FileSystem::append( directory_path_, IVF_FILENAME );
}

AlfalfaVideo::AlfalfaVideo( const string & directory_name, OpenMode mode )
  : directory_( FileSystem::get_realpath( directory_name ) ),
    video_manifest_( directory_.video_manifest_filename(), mode ),
    raster_list_( directory_.raster_list_filename(), mode ),
    quality_db_( directory_.quality_db_filename(), mode ),
    frame_db_( directory_.frame_db_filename(), mode ),
    track_db_( directory_.track_db_filename(), mode ),
    switch_db_( directory_.switch_db_filename(), mode ),
    switch_mappings_()
{}

AlfalfaVideo::AlfalfaVideo( AlfalfaVideo && other )
  : directory_( move( other.directory_ ) ),
    video_manifest_( move( other.video_manifest_ ) ),
    raster_list_( move( other.raster_list_ ) ),
    quality_db_( move( other.quality_db_ ) ),
    frame_db_( move( other.frame_db_ ) ),
    track_db_( move( other.track_db_ ) ),
    switch_db_( move( other.switch_db_ ) ),
    switch_mappings_( move( other.switch_mappings_ ) )
{}

size_t AlfalfaVideo::get_raster_list_size() const
{
  return raster_list_.size();
}

RasterData AlfalfaVideo::get_raster( const size_t raster_index ) const
{
  return raster_list_.raster( raster_index );
}

bool AlfalfaVideo::has_raster( const size_t raster ) const
{
  return raster_list_.has( raster );
}

pair<QualityDBCollectionSequencedAccess::iterator, QualityDBCollectionSequencedAccess::iterator>
AlfalfaVideo::get_quality_data() const
{
  return make_pair( quality_db_.begin(), quality_db_.end() );
}

pair<QualityDBCollectionByApproximateRaster::iterator, QualityDBCollectionByApproximateRaster::iterator>
AlfalfaVideo::get_quality_data_by_approximate_raster( const size_t approximate_raster ) const
{
  return quality_db_.search_by_approximate_raster( approximate_raster );
}

pair<QualityDBCollectionByOriginalRaster::iterator, QualityDBCollectionByOriginalRaster::iterator>
AlfalfaVideo::get_quality_data_by_original_raster( const size_t original_raster ) const
{
  return quality_db_.search_by_original_raster( original_raster );
}

pair<FrameDataSetCollectionByOutputHash::const_iterator, FrameDataSetCollectionByOutputHash::const_iterator>
AlfalfaVideo::get_frames_by_output_hash( const size_t output_hash ) const
{
  return frame_db_.search_by_output_hash( output_hash );
}

pair<FrameDataSetSourceHashSearch::iterator, FrameDataSetSourceHashSearch::iterator>
AlfalfaVideo::get_frames_by_decoder_hash( const DecoderHash & decoder_hash ) const
{
  return frame_db_.search_by_decoder_hash( decoder_hash );
}

pair<FrameDataSetCollectionSequencedAccess::iterator, FrameDataSetCollectionSequencedAccess::iterator>
AlfalfaVideo::get_frames() const
{
  return make_pair( frame_db_.begin(), frame_db_.end() );
}

pair<TrackDBCollectionSequencedAccess::iterator, TrackDBCollectionSequencedAccess::iterator>
AlfalfaVideo::get_track_data() const
{
  return make_pair( track_db_.begin(), track_db_.end() );
}

pair<SwitchDBCollectionSequencedAccess::iterator, SwitchDBCollectionSequencedAccess::iterator>
AlfalfaVideo::get_switch_data() const
{
  return make_pair( switch_db_.begin(), switch_db_.end() );
}

bool AlfalfaVideo::equal_raster_lists( const AlfalfaVideo & video ) const
{
  if ( get_raster_list_size() != video.get_raster_list_size() )
    return false;
  size_t i;
  for ( i = 0; i < get_raster_list_size(); i++ ) {
    if ( get_raster( i ).hash != video.get_raster( i ).hash )
      return false;
  }
  return true;
}

bool AlfalfaVideo::can_combine( const AlfalfaVideo & video ) const
{
  return (
    raster_list_.size() == 0 or
    (
      get_info() == video.get_info() and
      equal_raster_lists( video )
    )
  );
}

pair<unordered_set<size_t>::const_iterator, unordered_set<size_t>::const_iterator>
AlfalfaVideo::get_track_ids() const
{
  return track_db_.get_track_ids();
}

pair<unordered_set<size_t>::iterator, unordered_set<size_t>::iterator>
AlfalfaVideo::get_track_ids_for_switch(const size_t from_track_id, const size_t from_frame_index) const
{
  unordered_set<size_t> to_track_ids = switch_mappings_.at(
    make_pair( from_track_id, from_frame_index ) );
  return make_pair( to_track_ids.begin(), to_track_ids.end() );
}

pair<TrackDBIterator, TrackDBIterator>
AlfalfaVideo::get_frames( const size_t track_id ) const
{
  size_t start_frame_index = 0;
  size_t end_frame_index = track_db_.get_end_frame_index( track_id );
  return get_track_range( track_id, start_frame_index, end_frame_index );
}

pair<TrackDBIterator, TrackDBIterator>
AlfalfaVideo::get_frames( const TrackDBIterator & it ) const
{
  size_t track_id = it.track_id();
  size_t start_frame_index = it.frame_index();
  size_t end_frame_index = track_db_.get_end_frame_index( track_id );
  assert( start_frame_index <= end_frame_index );
  return get_track_range( track_id, start_frame_index, end_frame_index );
}

pair<TrackDBIterator, TrackDBIterator>
AlfalfaVideo::get_frames( const SwitchDBIterator & it ) const
{
  size_t track_id = it.to_track_id();
  size_t start_frame_index = it.to_frame_index();
  size_t end_frame_index = track_db_.get_end_frame_index( track_id );
  assert( start_frame_index <= end_frame_index );
  return get_track_range( track_id, start_frame_index, end_frame_index );
}

pair<SwitchDBIterator, SwitchDBIterator>
AlfalfaVideo::get_frames( const TrackDBIterator & it, const size_t to_track_id ) const
{
  size_t from_track_id = it.track_id();
  size_t from_frame_index = it.frame_index();
  return get_frames_switch( from_track_id, from_frame_index, to_track_id );
}

pair<SwitchDBIterator, SwitchDBIterator>
AlfalfaVideo::get_frames_switch( const size_t from_track_id,
                                 const size_t from_frame_index,
                                 const size_t to_track_id ) const
{
  size_t end_switch_frame_index = switch_db_.get_end_switch_frame_index( from_track_id,
                                                                         to_track_id,
                                                                         from_frame_index );
  return get_switch_range( from_track_id, to_track_id, from_frame_index, 0,
                           end_switch_frame_index );
}

pair<TrackDBIterator, TrackDBIterator>
AlfalfaVideo::get_frames_reverse( const size_t track_id, const size_t frame_index ) const
{
  size_t end_frame_index = track_db_.get_end_frame_index( track_id );
  assert( frame_index < end_frame_index );
  // To avoid build error when building with debug flag.
  (void) end_frame_index;
  return get_frames_reverse( track_id, frame_index, 0 );
}

pair<TrackDBIterator, TrackDBIterator>
AlfalfaVideo::get_frames_reverse( const size_t track_id, const size_t from_frame_index,
                                  const size_t to_frame_index ) const
{
  assert( to_frame_index <= from_frame_index );
  TrackDBIterator begin( track_id, (size_t) from_frame_index, track_db_, frame_db_ );
  TrackDBIterator end( track_id, to_frame_index - 1, track_db_, frame_db_ );
  return make_pair( begin, end );
}

vector<pair<SwitchDBIterator, SwitchDBIterator> >
AlfalfaVideo::get_switches_ending_with_frame( const size_t frame_id ) const
{
  auto switches = switch_db_.get_switches_by_frame_id( frame_id );
  vector<pair<SwitchDBIterator, SwitchDBIterator> > results;

  for ( auto sw = switches.first; sw != switches.second; sw++ ) {
    SwitchDBIterator begin(
      sw->from_track_id, sw->to_track_id, sw->from_frame_index,
      sw->switch_frame_index, switch_db_, frame_db_
    );

    SwitchDBIterator end(
      sw->from_track_id, sw->to_track_id, sw->from_frame_index,
      -1, switch_db_, frame_db_
    );

    results.push_back( make_pair( begin, end ) );
  }

  return results;
}

pair<TrackDBIterator, TrackDBIterator>
AlfalfaVideo::get_track_range( const size_t track_id,
                               const size_t start_frame_index,
                               const size_t end_frame_index ) const
{
  TrackDBIterator begin = TrackDBIterator( track_id, start_frame_index, track_db_, frame_db_ );
  TrackDBIterator end = TrackDBIterator( track_id, end_frame_index, track_db_, frame_db_ );

  return make_pair( begin, end );
}

std::pair<SwitchDBIterator, SwitchDBIterator>
AlfalfaVideo::get_switch_range( const size_t from_track_id,
                                const size_t to_track_id,
                                const size_t from_frame_index,
                                const size_t switch_start_index,
                                const size_t switch_end_index ) const
{
  SwitchDBIterator begin( from_track_id, to_track_id, from_frame_index,
    switch_start_index, switch_db_, frame_db_ );
  SwitchDBIterator end( from_track_id, to_track_id, from_frame_index,
    switch_end_index, switch_db_, frame_db_ );

  return make_pair( begin, end );
}

double AlfalfaVideo::get_quality( int raster_index, const FrameInfo & frame_info ) const
{
  size_t original_raster = raster_list_.raster( raster_index ).hash;
  size_t approximate_raster = frame_info.target_hash().output_hash;
  return quality_db_.search_by_original_and_approximate_raster(
    original_raster, approximate_raster ).quality;
}

vector<set<size_t> > AlfalfaVideo::build_frames_graph( bool dependency_graph )
{
  size_t N = frame_db_.size();
  vector<set<size_t> > A( N );
  auto keyframe_search = frame_db_.search_for_keyframes();

  for ( auto keyframe = keyframe_search.first; keyframe != keyframe_search.second; keyframe++ ) {
    DecoderHash decoder_state( 0, 0, 0, 0 );
    decoder_state.update( keyframe->target_hash() );

    forward_list<pair<size_t, DecoderHash> > Q;
    set<size_t> visited;

    Q.push_front( make_pair( keyframe->frame_id(), decoder_state ) );

    while ( not Q.empty() ) {
      pair<size_t, DecoderHash> current = Q.front();
      Q.pop_front();

      visited.insert( current.first );

      const FrameInfo & frame_info = frame_db_[ current.first ];
      bool is_keyframe = frame_info.is_keyframe();

      DecoderHash current_state = current.second;
      auto next_frames = frame_db_.search_by_decoder_hash( current_state );

      for ( auto it = next_frames.first; it != next_frames.second; it++ ) {
        if ( is_keyframe and it->is_keyframe() ) {
          continue;
        }

        if ( dependency_graph ) {
          A[ it->frame_id() ].insert( frame_info.frame_id() );
        }
        else {
          A[ frame_info.frame_id() ].insert( it->frame_id() );
        }

        if ( not it->is_keyframe() and visited.count( it->frame_id() ) == 0 ) {
          DecoderHash new_state = current_state;
          new_state.update( it->target_hash() );
          Q.push_front( make_pair( it->frame_id(), new_state ) );
        }
      }
    }
  }

  return A;
}

/* WritableAlfalfaVideo */

WritableAlfalfaVideo::WritableAlfalfaVideo( const string & directory_name,
                                            const string & fourcc,
                                            const uint16_t width, const uint16_t height )
  : AlfalfaVideo( directory_name, OpenMode::TRUNCATE ),
    ivf_writer_( directory_.ivf_filename(), fourcc, width, height, 24, 1 )
{
  video_manifest_.set_info( VideoInfo( fourcc, width, height ) );
}

void WritableAlfalfaVideo::insert_frame( FrameInfo next_frame,
                                         const size_t original_raster,
                                         const double quality,
                                         const size_t track_id )
{
  if ( next_frame.target_hash().shown ) {
    raster_list_.insert(
      RasterData{
        original_raster
      }
    );

    quality_db_.insert(
      QualityData{
        original_raster,
        next_frame.target_hash().output_hash,
        quality
      }
    );
  }

  size_t frame_id = frame_db_.insert( next_frame );

  track_db_.insert(
    TrackData{
      track_id,
      frame_id
    }
  );
}

FrameInfo WritableAlfalfaVideo::import_serialized_frame( const SerializedFrame & frame )
{
  if ( not frame_db_.has_frame_name( frame.name() ) ) {
    size_t offset = ivf_writer_.append_frame( frame.chunk() );
    FrameInfo fi( frame.name(), offset, frame.chunk().size() );

    frame_db_.insert( fi );

    return fi;
  }
  return frame_db_.search_by_frame_name( frame.name() );
}

void
WritableAlfalfaVideo::insert_switch_frames( const TrackDBIterator & origin_iterator,
                                            const std::vector<FrameInfo> & frames,
                                            const TrackDBIterator & dest_iterator )
{
  size_t from_track_id = origin_iterator.track_id();
  size_t from_frame_index = origin_iterator.frame_index();
  size_t to_track_id = dest_iterator.track_id();
  size_t to_frame_index = dest_iterator.frame_index();
  size_t switch_frame_index = 0;

  for (FrameInfo frame : frames) {
    size_t frame_id = frame_db_.insert( frame );
    SwitchData switch_data = SwitchData( from_track_id, from_frame_index, to_track_id,
                                         to_frame_index, frame_id, switch_frame_index );
    switch_db_.insert( switch_data );
    switch_frame_index++;
  }
}

size_t
WritableAlfalfaVideo::insert_frame_data( FrameInfo frame_info,
                                         const Chunk & chunk )
{
  if ( not frame_db_.has_frame_name( frame_info.name() ) ) {
    size_t offset = ivf_writer_.append_frame( chunk );
    frame_info.set_offset( offset );
  }
  return frame_db_.insert( frame_info );
}

void WritableAlfalfaVideo::save()
{
  video_manifest_.serialize();
  raster_list_.serialize();
  quality_db_.serialize();
  track_db_.serialize();
  frame_db_.serialize();
  switch_db_.serialize();
}

/* PlayableAlfalfaVideo */

PlayableAlfalfaVideo::PlayableAlfalfaVideo( const string & directory_name )
  : AlfalfaVideo( directory_name, OpenMode::READ ),
    ivf_file_( directory_.ivf_filename() )
{}

const Chunk PlayableAlfalfaVideo::get_chunk( const FrameInfo & frame_info ) const
{
  return ivf_file_( frame_info.offset(), frame_info.length() );
}

void PlayableAlfalfaVideo::encode( const size_t track_id, vector<string> vpxenc_args,
                                   const string & destination ) const
{
  TempFile fpf_file( "fpf" );
  const int total_passes = 2;

  // Newer libvpx versions default to vp9 so explicitly state that we want vp8
  vpxenc_args.push_back( "--codec=vp8" );

  // Adding fpf filename -- required for multiple passes
  vpxenc_args.push_back( "--fpf=" + fpf_file.name() );

  ostringstream vpxenc_command;
  vpxenc_command << VPXENC_EXECUTABLE << " ";

  for ( const auto & s : vpxenc_args ) {
    vpxenc_command << s <<  " ";
  }

  vpxenc_command << "--passes=" << total_passes << " ";

  for ( int pass = 1; pass <= total_passes; pass++ ) {
    ostringstream stage_command( vpxenc_command.str(), ios_base::app | ios_base::out );
    stage_command << "--pass=" << pass;
    // According to vpxenc spec, -o argument should be at the end
    stage_command << " -o " << destination << " -";

    Subprocess proc( stage_command.str(), "w" );

    FramePlayer player( get_info().width, get_info().height );

    /* yuv4mpeg header */
    proc.write( "YUV4MPEG2 W" + to_string( get_info().width )
                + " H" + to_string( get_info().height ) + " F24:1 " +
		+ "Ip A0:0 C420 C420jpeg XYSCSS=420JPEG\n" );

    for ( auto track_frames = get_frames( track_id ); track_frames.first != track_frames.second; track_frames.first++ ) {
      Optional<RasterHandle> raster_handle = player.decode( get_chunk( *track_frames.first ) );

      if ( raster_handle.initialized() ) {
	const string frame_tag { "FRAME\n" };
        proc.write( frame_tag );

	for ( const auto & chunk : raster_handle.get().get().display_rectangle_as_planar() ) {
	  proc.write( chunk );
	}
      }
    }

    proc.close(); /* may throw an exception, so call explicitly instead of letting destructor do it */
  }
}

void
combine( WritableAlfalfaVideo & combined_video, const PlayableAlfalfaVideo & video )
{
  if ( not combined_video.can_combine( video ) ) {
    throw invalid_argument( "cannot combine: raster lists are not the same." );
  }
  else if ( combined_video.get_raster_list_size() == 0 ) {
    size_t i;
    for ( i = 0; i < video.get_raster_list_size(); i++ ) {
      combined_video.insert_raster( video.get_raster( i ) );
    }
  }

  for ( auto quality_data = video.get_quality_data();
        quality_data.first != quality_data.second; quality_data.first++ ) {
    combined_video.insert_quality_data( *quality_data.first );
  }

  map<size_t, size_t> frame_id_mapping;
  map<size_t, size_t> track_id_mapping;

  for ( auto frame_data = video.get_frames();
        frame_data.first != frame_data.second; frame_data.first++ ) {
    FrameInfo frame_info = *frame_data.first;
    frame_id_mapping[ frame_info.frame_id() ] = combined_video.insert_frame_data(
      frame_info, video.get_chunk( frame_info ) );
  }

  for ( auto track_data = video.get_track_data();
        track_data.first != track_data.second; track_data.first++ ) {
    TrackData item = *track_data.first;
    if ( track_id_mapping.count( item.track_id ) > 0 ) {
      item.track_id = track_id_mapping[ item.track_id ];
    }
    else if ( combined_video.has_track( item.track_id ) ) {
      size_t new_track_id = item.track_id;
      while ( combined_video.has_track( new_track_id ) ) {
        new_track_id++;
      }
      track_id_mapping[ item.track_id ] = new_track_id;
      item.track_id = new_track_id;
    } else {
      track_id_mapping[ item.track_id ] = item.track_id;
    }
    item.frame_id = frame_id_mapping[ item.frame_id ];
    combined_video.insert_track_data( item );
  }

  for ( auto switch_data = video.get_switch_data();
        switch_data.first != switch_data.second; switch_data.first++ ) {
    SwitchData item = *switch_data.first;
    item.from_track_id = track_id_mapping[ item.from_track_id ];
    item.to_track_id = track_id_mapping[ item.to_track_id ];
    item.frame_id = frame_id_mapping[ item.frame_id ];
    combined_video.insert_switch_data( item );
  }
}
