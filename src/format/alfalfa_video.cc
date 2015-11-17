#include <cstdio>
#include <sstream>
#include <algorithm>
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

#define VPXENC_EXECUTABLE "vpxenc"

using namespace std;

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
    video_manifest_( directory_.video_manifest_filename(), "ALFAVDMF", mode ),
    raster_list_( directory_.raster_list_filename(), "ALFARSLS", mode ),
    quality_db_( directory_.quality_db_filename(), "ALFAQLDB", mode ),
    frame_db_( directory_.frame_db_filename(), "ALFAFRDB", mode ),
    track_db_( directory_.track_db_filename(), "ALFATRDB", mode ),
    switch_db_( directory_.switch_db_filename(), "ALFASWDB", mode ),
    switch_mappings_()
{
  if ( not good() ) {
    throw invalid_argument( "invalid alfalfa video." );
  }
}

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

bool AlfalfaVideo::good() const
{
  return video_manifest_.good() and raster_list_.good() and quality_db_.good()
    and frame_db_.good() and track_db_.good();
}

bool AlfalfaVideo::can_combine( const AlfalfaVideo & video )
{
  return (
    raster_list_.size() == 0 or
    (
      video_manifest().info() == video.video_manifest().info() and
      raster_list_ == video.raster_list_
    )
  );
}

pair<unordered_set<size_t>::iterator, unordered_set<size_t>::iterator>
AlfalfaVideo::get_track_ids()
{
  return track_db_.get_track_ids();
}

pair<unordered_set<size_t>::iterator, unordered_set<size_t>::iterator>
AlfalfaVideo::get_track_ids_for_switch(const size_t from_track_id, const size_t from_frame_index)
{
  unordered_set<size_t> to_track_ids = switch_mappings_.at(
    make_pair( from_track_id, from_frame_index ) );
  return make_pair( to_track_ids.begin(), to_track_ids.end() );
}

pair<FrameDataSetCollectionSequencedAccess::iterator, FrameDataSetCollectionSequencedAccess::iterator>
AlfalfaVideo::get_frames() const
{
  return make_pair( frame_db_.begin(), frame_db_.end() );
}

pair<TrackDBIterator, TrackDBIterator>
AlfalfaVideo::get_frames( const size_t track_id )
{
  size_t end_frame_index = track_db_.get_end_frame_index( track_id );
  TrackDBIterator begin = TrackDBIterator( track_id, 0, track_db_, frame_db_ );
  TrackDBIterator end = TrackDBIterator( track_id, end_frame_index, track_db_, frame_db_);
  return make_pair( begin, end );
}

pair<TrackDBIterator, TrackDBIterator>
AlfalfaVideo::get_frames( const TrackDBIterator & it )
{
  size_t track_id = it.track_id();
  size_t start_frame_index = it.frame_index();
  size_t end_frame_index = track_db_.get_end_frame_index( track_id );
  assert( start_frame_index <= end_frame_index );
  TrackDBIterator begin = TrackDBIterator( track_id, start_frame_index, track_db_, frame_db_ );
  TrackDBIterator end = TrackDBIterator( track_id, end_frame_index, track_db_, frame_db_);
  return make_pair( begin, end );
}

pair<TrackDBIterator, TrackDBIterator>
AlfalfaVideo::get_frames( const SwitchDBIterator & it )
{
  size_t track_id = it.to_track_id();
  size_t start_frame_index = it.to_frame_index();
  size_t end_frame_index = track_db_.get_end_frame_index( track_id );
  assert( start_frame_index <= end_frame_index );
  TrackDBIterator begin = TrackDBIterator( track_id, start_frame_index, track_db_, frame_db_ );
  TrackDBIterator end = TrackDBIterator( track_id, end_frame_index, track_db_, frame_db_);
  return make_pair( begin, end );
}

pair<SwitchDBIterator, SwitchDBIterator>
AlfalfaVideo::get_frames( const TrackDBIterator & it, const size_t to_track_id )
{
  size_t from_track_id = it.track_id();
  size_t from_frame_index = it.frame_index();
  size_t end_switch_frame_index = switch_db_.get_end_switch_frame_index( from_track_id,
                                                                         to_track_id,
                                                                         from_frame_index );
  SwitchDBIterator begin = SwitchDBIterator( from_track_id, to_track_id,
                                             from_frame_index, 0, switch_db_, frame_db_ );
  SwitchDBIterator end = SwitchDBIterator( from_track_id, to_track_id,
                                           from_frame_index, end_switch_frame_index,
                                           switch_db_, frame_db_ );
  return make_pair( begin, end );
}

double AlfalfaVideo::get_quality( int raster_index, const FrameInfo & frame_info )
{
  size_t original_raster = raster_list_.raster( raster_index );
  size_t approximate_raster = frame_info.target_hash().output_hash;
  return quality_db_.search_by_original_and_approximate_raster(
    original_raster, approximate_raster ).quality;
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

WritableAlfalfaVideo::WritableAlfalfaVideo( const string & directory_name,
                                            const IVF & ivf )
  : WritableAlfalfaVideo( directory_name, ivf.fourcc(), ivf.width(), ivf.height() )
{}

WritableAlfalfaVideo::WritableAlfalfaVideo( const string & directory_name,
                                            const VideoInfo & info )
  : WritableAlfalfaVideo( directory_name, info.fourcc, info.width, info.height )
{}

void WritableAlfalfaVideo::combine( const PlayableAlfalfaVideo & video )
{
  if ( not can_combine( video ) ) {
    throw invalid_argument( "cannot combine: raster lists are not the same." );
  }
  else if ( raster_list_.size() == 0 ) {
    raster_list_.merge( video.raster_list() );
  }

  video_manifest_.set_info( video.video_manifest().info() );
  quality_db_.merge( video.quality_db() );
  map<size_t, size_t> frame_id_mapping;

  for ( auto item = video.get_frames().first; item != video.get_frames().second; item++ ) {
    size_t offset = ivf_writer_.append_frame( video.get_chunk( *item ) );
    FrameInfo frame_info = *item;
    frame_info.set_offset( offset );
    frame_id_mapping[ item->frame_id() ] = frame_db_.insert( *item );
  }

  track_db_.merge( video.track_db(), frame_id_mapping );
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

void WritableAlfalfaVideo::write_ivf( const string & filename )
{
  IVF ivf_file( filename );

  for ( unsigned int i = 0; i < ivf_file.frame_count(); i++ ) {
    ivf_writer_.append_frame( ivf_file.frame( i ) );
  }
}

size_t WritableAlfalfaVideo::import_frame( FrameInfo fi, const Chunk & chunk )
{
  size_t offset = ivf_writer_.append_frame( chunk );
  fi.set_offset( offset );

  SourceHash source_hash = fi.source_hash();
  TargetHash target_hash = fi.target_hash();

  if ( not frame_db_.has_frame_name( source_hash, target_hash ) ) {
    return frame_db_.insert( fi );
  }
  return frame_db_.search_by_frame_name( source_hash, target_hash ).frame_id();
}

void WritableAlfalfaVideo::import( const string & filename )
{
  write_ivf( filename );
  TrackingPlayer player( directory_.ivf_filename() );

  size_t track_id = 0;

  while ( not player.eof() ) {
    auto next_frame_data = player.serialize_next();
    FrameInfo next_frame( next_frame_data.first );

    size_t original_raster = next_frame.target_hash().output_hash;
    double quality = 1.0;

    insert_frame( next_frame, original_raster, quality, track_id );
  }

  save();
}

void WritableAlfalfaVideo::import( const string & filename,
                                   PlayableAlfalfaVideo & original,
                                   const size_t ref_track_id )
{
  write_ivf( filename );
  TrackingPlayer player( directory_.ivf_filename() );

  FramePlayer original_player( original.video_manifest().width(), original.video_manifest().height() );
  auto track_frames = original.get_frames( ref_track_id );

  size_t track_id = 0;

  while ( not player.eof() ) {
    auto next_frame_data = player.serialize_next();
    FrameInfo next_frame( next_frame_data.first );

    size_t original_raster = next_frame.target_hash().output_hash;
    double quality = 1.0;

    if ( next_frame.target_hash().shown and next_frame_data.second.initialized() ) {
      while ( track_frames.first != track_frames.second and not track_frames.first->shown() ) {
        track_frames.first++;
      }

      if ( track_frames.first != track_frames.second ) {
        auto original_uncompressed_frame = original_player.decode(
          original.get_chunk( *track_frames.first ) );

        quality = original_uncompressed_frame.get().get().quality(
          next_frame_data.second.get() );

        original_raster = track_frames.first->target_hash().output_hash;

        track_frames.first++;
      }
      else {
        throw logic_error( "bad original video" );
      }
    }
    else if ( not next_frame_data.second.initialized() ) {
      throw invalid_argument( "inconsistent frame db" );
    }

    insert_frame( next_frame, original_raster, quality, track_id );
  }

  save();
}

bool WritableAlfalfaVideo::save()
{
  return video_manifest_.serialize() and
    raster_list_.serialize() and
    quality_db_.serialize() and
    track_db_.serialize() and
    frame_db_.serialize();
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
                                   const string & destination )
{
  TempFile fpf_file( "fpf" );
  int total_passes = 2;

  // adding necessary output switches
  vpxenc_args.push_back( "-o" );
  vpxenc_args.push_back( destination );
  vpxenc_args.push_back( "-" );
  vpxenc_args.push_back( "--fpf=" + fpf_file.name() );

  ostringstream vpxenc_command;
  vpxenc_command << VPXENC_EXECUTABLE << " ";

  for_each( vpxenc_args.begin(), vpxenc_args.end(), [ &vpxenc_command ]( string & s ) {
      vpxenc_command << s <<  " ";
    }
  );

  for ( int pass = 1; pass <= total_passes; pass++ ) {
    vector<string> full_args = vpxenc_args;

    ostringstream stage_command( vpxenc_command.str(), ios_base::app | ios_base::out );
    stage_command << "--passes=" << total_passes
                  << " --pass=" << pass;

    Subprocess proc;
    proc.call( stage_command.str(), "w" );

    FramePlayer player( video_manifest().width(), video_manifest().height() );

    ostringstream ss;
    ss << "YUV4MPEG2 W" << video_manifest().width() << " "
      << "F1:1 "
      << "H" << video_manifest().height() << " "
      << "Ip A0:0 C420 C420jpeg XYSCSS=420JPEG\n";

    string header( ss.str() );
    proc.write_string( header );

    for( auto track_frames = get_frames( track_id ); track_frames.first != track_frames.second; track_frames.first++ ) {
      Optional<RasterHandle> uncompressed_frame = player.decode( get_chunk( *track_frames.first ) );

      if ( uncompressed_frame.initialized() ) {
        proc.write_string( "FRAME\n" );
        uncompressed_frame.get().get().dump( proc.stream() );
      }
    }

    int status = proc.wait();

    if ( status != 0 ) {
      throw runtime_error( "encode failed" );
    }
  }
}
