#include <cstdio>
#include <sstream>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "exception.hh"
#include "alfalfa_video.hh"
#include "tracking_player.hh"
#include "filesystem.hh"
#include "system_runner.hh"

AlfalfaVideo::VideoDirectory::VideoDirectory( const std::string & path )
  : directory_path_( path )
{}

std::string AlfalfaVideo::VideoDirectory::video_manifest_filename() const
{
  return FileSystem::append( directory_path_, VIDEO_MANIFEST_FILENAME );
}

std::string AlfalfaVideo::VideoDirectory::raster_list_filename() const
{
  return FileSystem::append( directory_path_, RASTER_LIST_FILENAME );
}

std::string AlfalfaVideo::VideoDirectory::quality_db_filename() const
{
  return FileSystem::append( directory_path_, QUALITY_DB_FILENAME );
}

std::string AlfalfaVideo::VideoDirectory::frame_db_filename() const
{
  return FileSystem::append( directory_path_, FRAME_DB_FILENAME );
}

std::string AlfalfaVideo::VideoDirectory::track_db_filename() const
{
  return FileSystem::append( directory_path_, TRACK_DB_FILENAME );
}

AlfalfaVideo::AlfalfaVideo( const string & directory_name, OpenMode mode )
  : directory_( FileSystem::get_realpath( directory_name ) ),
    video_manifest_( directory_.video_manifest_filename(), "ALFAVDMF", mode ),
    raster_list_( directory_.raster_list_filename(), "ALFARSLS", mode ),
    quality_db_( directory_.quality_db_filename(), "ALFAQLDB", mode ),
    frame_db_( directory_.frame_db_filename(), "ALFAFRDB", mode ),
    track_db_( directory_.track_db_filename(), "ALFATRDB", mode ),
    track_ids_(),
    ivf_file_()
{
  if ( not good() ) {
    throw Invalid( "invalid alfalfa video." );
  }

  if ( mode == OpenMode::READ ) {
    ivf_file_.reset( new File( FileSystem::append( directory_.path(), get_ivf_file_name() ) ) );
  }
}

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
      video_manifest_.width() == video.video_manifest_.width() and
      video_manifest_.height() == video.video_manifest_.height() and
      raster_list_ == video.raster_list_
    )
  );
}

void AlfalfaVideo::combine( const AlfalfaVideo & video, IVFWriter & combined_ivf_writer )
{
if ( not can_combine( video ) ) {
    throw Invalid( "cannot combine: raster lists are not the same." );
  }
  else if ( raster_list_.size() == 0 ) {
    raster_list_.merge( video.raster_list_ );
  }
  video_manifest_.set_info( video.video_manifest_.info() );
  quality_db_.merge( video.quality_db_ );
  map<size_t, size_t> frame_id_mapping;

  const string ivf_file_path = FileSystem::append( video.directory_.path(), get_ivf_file_name() );
  frame_db_.merge( video.frame_db_, frame_id_mapping, combined_ivf_writer, ivf_file_path );

  track_db_.merge( video.track_db_, frame_id_mapping );
  for (auto it = track_db_.begin(); it != track_db_.end(); it++) {
    track_ids_.insert( it->track_id );
  }

  ivf_file_.reset( new File( FileSystem::append( directory_.path(), get_ivf_file_name() ) ) );
}

std::string AlfalfaVideo::encode( const size_t & track_id,
  vector<string> vpxenc_args )
{
  std::string encoded_file = std::tmpnam( nullptr );
  std::string fpf_file = std::tmpnam( nullptr );
  int pipefd[2];
  int total_passes = 2;
  pid_t cpid;

  // adding necessary output switches
  vpxenc_args.push_back( "-o" );
  vpxenc_args.push_back( encoded_file );
  vpxenc_args.push_back( "-" );
  vpxenc_args.push_back( "--fpf=" + fpf_file );

  for ( int pass = 1; pass <= total_passes; pass++ ) {
    vector<string> full_args = vpxenc_args;

    ostringstream ss;
    ss << "--passes=" << total_passes;
    full_args.push_back( ss.str() );

    ss.str( std::string() );
    ss << "--pass=" << pass;
    full_args.push_back( ss.str() );

    SystemCall( "encode pipe", pipe( pipefd ) );
    cpid = SystemCall( "encode fork", fork() );

    if ( cpid == 0 ) {
      SystemCall( "close stdin", close( STDIN_FILENO ) );
      SystemCall( "close pipe write", close( pipefd[ 1 ] ) );
      SystemCall( "dup stdin", dup( pipefd[ 0 ] ) );
      SystemCall( "close pipe read", close( pipefd[ 0 ] ) );
      ezexec( full_args, true );
    }
    else {
      SystemCall( "close pipe read", close( pipefd[ 0 ] ) );

      FramePlayer player( video_manifest().width(), video_manifest().height() );

      ostringstream ss;
      ss << "YUV4MPEG2 W" << video_manifest().width() << " "
        << "H" << video_manifest().height() << " "
        << "F" << video_manifest().frame_rate_numerator() << ":" << video_manifest().frame_rate_denominator() << " "
        << "Ip A0:0 C420 C420jpeg XYSCSS=420JPEG\n";

      std::string header( ss.str() );
      SystemCall( "write header", write( pipefd[ 1 ], header.c_str(), header.length() ) );

      size_t frame_index = 0;
      for ( Optional<FrameInfo> frame = get_next_frame_info( track_id, frame_index );
           frame.initialized();
           frame = get_next_frame_info( track_id, ++frame_index ) )
      {
        Optional<RasterHandle> uncompressed_frame = player.decode( get_chunk( frame.get() ) );

        if ( uncompressed_frame.initialized() ) {
          SystemCall( "write frame header", write( pipefd[ 1 ], "FRAME\n", strlen("FRAME\n") ) );
          uncompressed_frame.get().get().dump( pipefd[ 1 ] );
        }
      }

      SystemCall( "close pipe write", close( pipefd[ 1 ] ) );
    }

    int status;
    SystemCall( "wait", wait( &status ) );

    if ( status != 0 ) {
      throw runtime_error( "encode failed" );
    }
  }

  return encoded_file;
}

void AlfalfaVideo::import( const string & filename,
  Optional<AlfalfaVideo> original, const size_t & track_id )
{
  bool has_original = original.initialized();

  TrackingPlayer player( FileSystem::append( directory_.path(), filename ) );

  IVF ivf( FileSystem::append( directory_.path(), filename ) );
  ivf_file_.reset( new File( FileSystem::append( directory_.path(), filename ) ) );

  Optional<FramePlayer> original_player;

  if ( has_original ) {
    original_player.initialize( FramePlayer( original.get().video_manifest().width(),
      original.get().video_manifest().height() ) );
  }

  VideoInfo info( ivf.fourcc(), ivf.width(), ivf.height(), ivf.frame_rate(),
    ivf.time_scale(), ivf.frame_count() );

  video_manifest_.set_info( info );

  size_t original_frame_index = 0;
  size_t frame_index = 1;
  size_t frame_id = 0;

  while ( not player.eof() ) {
    auto next_frame_data = player.serialize_next();
    FrameInfo next_frame( next_frame_data.first );

    if ( next_frame.target_hash().shown ) {
      assert( next_frame.target_hash().shown == next_frame_data.second.initialized() );

      size_t output_hash = next_frame.target_hash().output_hash;
      double quality = 1.0;

      if ( has_original ) {
        Optional<FrameInfo> original_frame;
        for ( original_frame = original.get().get_next_frame_info( track_id, original_frame_index );
              original_frame.initialized() and not original_frame.get().shown();
              original_player.get().decode( original.get().get_chunk( original_frame.get() ) ),
              original_frame = original.get().get_next_frame_info( track_id, ++original_frame_index ) )
        {}

        if ( original_frame.initialized() ) {
          auto original_uncompressed_frame = original_player.get().decode(
            original.get().get_chunk( original_frame.get() ) );

          original_frame_index++;

          assert( original_frame.get().shown() == original_uncompressed_frame.initialized() );

          quality = original_uncompressed_frame.get().get().quality(
            next_frame_data.second.get() );

          output_hash = original_frame.get().target_hash().output_hash;
        }
        else {
          throw logic_error( "bad original video" );
        }
      }
      else {
        output_hash = next_frame.target_hash().output_hash;
      }

      raster_list_.insert(
        RasterData{
          output_hash
        }
      );

      quality_db_.insert(
        QualityData{
          output_hash,
          next_frame.target_hash().output_hash,
          quality
        }
      );
    }

    size_t latest_frame_id;
    SourceHash source_hash = next_frame.source_hash();
    TargetHash target_hash = next_frame.target_hash();
    next_frame.set_index( frame_index - 1 );
    if ( not frame_db_.has_frame_name( source_hash, target_hash ) ) {
      next_frame.set_frame_id( frame_id );
      latest_frame_id = frame_id;
      frame_id++;
      frame_db_.insert( next_frame );
    } else {
      latest_frame_id = frame_db_.search_by_frame_name( source_hash,
        target_hash ).frame_id();
    }

    size_t track_id = 0;
    track_db_.insert(
      TrackData{
        track_id,
        frame_index,
        latest_frame_id
      }
    );

    track_ids_.insert( track_id );

    frame_index++;
  }

  save();
}

std::pair<std::unordered_set<size_t>::iterator, std::unordered_set<size_t>::iterator>
AlfalfaVideo::get_track_ids () {
  return make_pair( track_ids_.begin(), track_ids_.end() );
}

FrameInfo
AlfalfaVideo::get_first_frame( const size_t & track_id ) {
  if ( not track_db_.has_track_id( track_id ) )
    throw std::out_of_range( "Track id doesn't exist!" );
  auto it = track_db_.search_by_track_id( track_id );
  return frame_db_.search_by_frame_id( it.first->frame_id );
}

Optional<FrameInfo>
AlfalfaVideo::get_next_frame_info( const size_t & track_id, const size_t & frame_index ) {
  if ( not track_db_.has_track_id( track_id ) )
    throw std::out_of_range( "Track id doesn't exist!" );
  Optional<TrackData> track_data = track_db_.search_next_frame(
    track_id, frame_index );
  if ( not track_data.initialized() )
    return Optional<FrameInfo>();
  return make_optional(
    true,
    frame_db_.search_by_frame_id( track_data.get().frame_id )
  );
}

const Chunk AlfalfaVideo::get_chunk( const FrameInfo & frame_info ) const
{
  if ( ivf_file_ == nullptr ) {
    throw logic_error( "no ivf file associated with this video." );
  }

  return ( *ivf_file_ )( frame_info.offset(), frame_info.length() );
}

double
AlfalfaVideo::get_quality( int raster_index, const FrameInfo & frame_info ) {
  size_t original_raster = raster_list_.raster( raster_index );
  size_t approximate_raster = frame_info.target_hash().output_hash;
  return quality_db_.search_by_original_and_approximate_raster(
    original_raster, approximate_raster ).quality;
}

bool AlfalfaVideo::save()
{
  return video_manifest_.serialize() and
    raster_list_.serialize() and
    quality_db_.serialize() and
    track_db_.serialize() and
    frame_db_.serialize();
}
