/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <cstdlib>
#include <iostream>
#include <chrono>
#include <vector>

#include "yuv4mpeg.hh"
#include "encoder.hh"

using namespace std;

void usage( const char *argv0 )
{
  cerr << "Usage: " << argv0 << " INPUT QUANTIZER HOST PORT" << endl;
}

unsigned int paranoid_atoi( const string & in )
{
  const unsigned int ret = stoul( in );
  const string roundtrip = to_string( ret );
  if ( roundtrip != in ) {
    throw runtime_error( "invalid unsigned integer: " + in );
  }
  return ret;
}

int main( int argc, char *argv[] )
{
  /* check the command-line arguments */
  if ( argc < 1 ) { /* for sticklers */
    abort();
  }

  if ( argc != 5 ) {
    usage( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  /* open the YUV4MPEG input */
  YUV4MPEGReader input { argv[ 1 ] };

  /* get quantizer argument */
  const unsigned int y_ac_qi = paranoid_atoi( argv[ 2 ] );

  /* construct the encoder */
  Encoder encoder { input.display_width(), input.display_height(), false /* two-pass */, REALTIME_QUALITY };

  /* encode frames to stdout */
  unsigned int frame_no = 0;
  for ( Optional<RasterHandle> raster = input.get_next_frame();
        raster.initialized();
        raster = input.get_next_frame() ) {
    cerr << "Encoding frame #" << frame_no++ << "...";

    const auto encode_beginning = chrono::system_clock::now();
    vector<uint8_t> frame = encoder.encode_with_quantizer( raster.get(), y_ac_qi );
    const auto encode_ending = chrono::system_clock::now();
    const int ms_elapsed = chrono::duration_cast<chrono::milliseconds>( encode_ending - encode_beginning ).count();
    cerr << "done (" << ms_elapsed << " ms, size=" << frame.size() << ")." << endl;
  }

  return EXIT_SUCCESS;
}
