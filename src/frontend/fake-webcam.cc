/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <iostream>
#include <chrono>
#include <thread>

#include "yuv4mpeg.hh"
#include "paranoid.hh"

using namespace std;

void usage( const char *argv0 )
{
  cerr << "Usage: " << argv0 << " INPUT FPS" << endl;
}

int main( int argc, char *argv[] )
{
  /* check the command-line arguments */
  if ( argc < 1 ) { /* for sticklers */
    abort();
  }

  if ( argc != 3 ) {
    usage( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  /* open the YUV4MPEG input */
  YUV4MPEGReader input { argv[ 1 ] };

  /* parse the # of frames per second of playback */
  unsigned int frames_per_second = paranoid::stoul( argv[ 2 ] );

  /* open the output */
  FileDescriptor stdout { STDOUT_FILENO };

  const auto interval_between_frames = chrono::microseconds( int( 1.0e6 / frames_per_second ) );

  auto next_frame_is_due = chrono::system_clock::now();

  bool initialized = false;
  while ( true ) {
    /* wait until next frame is due */
    this_thread::sleep_until( next_frame_is_due );
    next_frame_is_due += interval_between_frames;

    /* get the next frame to send */
    const Optional<RasterHandle> raster = input.get_next_frame();
    if ( not raster.initialized() ) {
      break; /* eof */
    }

    /* send the file header if we haven't already */
    if ( not initialized ) {
      auto file_header = YUV4MPEGHeader( raster.get() );
      file_header.fps_numerator = frames_per_second;
      file_header.fps_denominator = 1;
      stdout.write( file_header.to_string() );
      initialized = true;
    }

    /* send the frame */
    YUV4MPEGFrameWriter::write( raster.get(), stdout );
  }
}
