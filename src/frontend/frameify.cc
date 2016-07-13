/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include "tracking_player.hh"

using namespace std;

class FrameDumper
{
private:
  // FrameDumper is constructed after we chdir into the video directory,
  // so these files are created inside the directory
  ofstream original_manifest_ { "original_manifest" };
  ofstream quality_manifest_ { "quality_manifest" };
  ofstream frame_manifest_ { "frame_manifest" };
  ofstream stream_manifest_ { "stream_manifest" };

  unordered_set<string> frame_names_ {};
  unordered_map<size_t, unordered_set<size_t>> recorded_qualities_ {};
  vector<TrackingPlayer> streams_ {};

  TrackingPlayer original_player_;

  void write_frame( const SerializedFrame & frame )
  {
    string frame_name = frame.name();

    // FIXME, ideally we could guarantee that write_frame was never
    // called with dups and could avoid this check
    if ( not frame_names_.count( frame_name ) ) {
      frame_names_.insert( frame_name );
      frame_manifest_ << frame_name << " " << frame.size() << endl;
      frame.write();
    }
  }

  void record_quality( const SerializedFrame & frame, const RasterHandle & original )
  {
    if ( not recorded_qualities_[ original.hash() ].count( frame.get_output().hash() ) ) {
      recorded_qualities_[ original.hash() ].insert( frame.get_output().hash() );
      quality_manifest_ << original.hash() << " " << frame.get_output().hash() << " " <<
        frame.quality( original ) << endl;
    }
  }

  void record_original( const RasterHandle & original )
  {
    original_manifest_ << original.hash() << endl;
  }

  void record_frame_stream( const SerializedFrame & frame, unsigned stream_idx )
  {
    stream_manifest_ << stream_idx << " " << frame.name() << endl;
  }

  SerializedFrame serialize_until_shown( TrackingPlayer & stream, unsigned stream_idx )
  {
    /* Serialize and write until next displayed frame */
    while ( not stream.eof() ) {
      SerializedFrame frame = stream.serialize_next().first; 
      write_frame( frame );
      record_frame_stream( frame, stream_idx );

      if ( frame.shown() ) {
        return frame;
      }
    }
    throw Unsupported( "Undisplayed frames at end of video unsupported" );
  }

public:
  FrameDumper( string original, const vector<string> & streams )
    : original_player_( original )
  {
    // FIXME when upscaling is implemented we shouldn't need to write out the width and height
    original_manifest_ << original_player_.width() << " " << original_player_.height() << endl;

    // Format the manifest hashes in hex
    original_manifest_ << uppercase << hex;
    quality_manifest_ << uppercase << hex << fixed;

    // Maximum precision when writing out SSIM
    quality_manifest_.precision( numeric_limits<double>::max_digits10 );

    for ( const string & stream : streams ) {
      streams_.emplace_back( stream );
    }
  }

  void output_frames( void )
  {
    // Serialize out the original stream
    while ( not original_player_.eof() ) {
      SerializedFrame original_frame = serialize_until_shown( original_player_, 0 );
      RasterHandle original_raster = original_frame.get_output();
      record_original( original_raster );
      record_quality( original_frame, original_raster );

      // Serialize out the approximation streams
      for ( unsigned stream_idx = 0; stream_idx < streams_.size(); stream_idx++ ) {
        TrackingPlayer & stream = streams_[ stream_idx ];

        SerializedFrame shown_frame = serialize_until_shown( stream, stream_idx + 1 );
        assert( shown_frame.shown() );

        record_quality( shown_frame, original_raster );
      }
    }
  }
};

int main( int argc, const char * const argv[] )
{
  if ( argc < 4 ) {
    cerr << "Usage: " << argv[ 0 ] << " OUTPUT_DIR ORIGINAL STREAM1 [ MORE_STREAMS ]" << endl;
    return EXIT_FAILURE;
  }
  char absolute[ PATH_MAX ];
  if ( not realpath( argv[ 2 ], absolute ) ) {
    throw unix_error( "realpath failed" );
  }
  string original( absolute );

  // Calculate absolute paths for all arguments before we chdir into OUTPUT_DIR
  vector<string> absolute_streams;
  // Argument list is null terminated
  for ( int i = 3; i < argc; i++ ) {
    if ( not realpath( argv[ i ], absolute ) ) {
      throw unix_error( "realpath failed" );
    }
    absolute_streams.emplace_back( absolute );
  }

  if ( mkdir( argv[ 1 ], 0777 ) ) {
    throw unix_error( "mkdir failed" );
  }
  if ( chdir( argv[ 1 ] ) ) {
    throw unix_error( "chdir failed" );
  }
  FrameDumper dumper( original, absolute_streams );
  dumper.output_frames();
}
