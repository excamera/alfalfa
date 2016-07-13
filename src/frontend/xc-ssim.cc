/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <getopt.h>
#include <iostream>

#include "ssim.hh"
#include "frame_input.hh"
#include "yuv4mpeg.hh"
#include "ivf_reader.hh"
#include "raster_handle.hh"

using namespace std;

void usage_error( const string & program_name )
{
  cerr << "Usage: " << program_name << " [options] <video1> <video2>" << endl
       << endl
       << "Options:" << endl
       << " -a,       --all-planes                Output SSIM for all planes" << endl
       << " -1 <arg>, --video1-format=<arg>       First video input format"   << endl
       << " -2 <arg>, --video2-format=<arg>       Second video input format"  << endl
       << "                                         ivf (default), y4m"       << endl;
}


int main( int argc, char *argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  if ( argc < 3 ) {
    usage_error( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  string video_format[ 2 ];
  bool all_planes = false;

  const option command_line_options[] = {
    { "all-planes",                no_argument, nullptr, 'a' },
    { "video1-format",       required_argument, nullptr, '1' },
    { "video2-format",       required_argument, nullptr, '2' },
    { 0, 0, nullptr, 0 }
  };

  while ( true ) {
    const int opt = getopt_long( argc, argv, "1:2:a", command_line_options, nullptr );

    if ( opt == -1 ) {
      break;
    }

    switch ( opt ) {
    case '1':
      video_format[ 0 ] = optarg;
      break;

    case '2':
      video_format[ 1 ] = optarg;
      break;

    case 'a':
      all_planes = true;
      break;

    default:
      throw runtime_error( "getopt_long: unexpected return value." );
    }
  }

  if ( optind + 1 >= argc ) {
    usage_error( argv[ 0 ] );
  }

  string video_file[ 2 ];
  video_file[ 0 ] = argv[ optind ];
  video_file[ 1 ] = argv[ optind + 1 ];
  shared_ptr<FrameInput> video_reader[ 2 ];

  for ( size_t i = 0; i < 2; i++ ) {
    if ( video_format[ i ] == "ivf" ) {
      video_reader[ i ] = make_shared<IVFReader>( video_file[ i ] );
    }
    else if ( video_format[ i ] == "y4m" ) {
      video_reader[ i ] = make_shared<YUV4MPEGReader>( video_file[ i ] );
    }
    else {
      throw runtime_error( "unsupported input format" );
    }
  }

  Optional<RasterHandle> raster[] = { video_reader[ 0 ]->get_next_frame(),
                                      video_reader[ 1 ]->get_next_frame() };

  while ( raster[ 0 ].initialized() and raster[ 1 ].initialized() ) {
    double y_ssim = ssim( raster[ 0 ].get().get().Y(), raster[ 1 ].get().get().Y() );
    cout << y_ssim;

    if ( all_planes ) {
      double u_ssim = ssim( raster[ 0 ].get().get().U(), raster[ 1 ].get().get().U() );
      double v_ssim = ssim( raster[ 0 ].get().get().V(), raster[ 1 ].get().get().V() );
      cout << "\t" << u_ssim << "\t" << v_ssim;
    }

    cout << endl;

    for ( size_t i = 0; i < 2; i++ ) {
      raster[ i ] = video_reader[ i ]->get_next_frame();
    }
  }

  return EXIT_SUCCESS;
}
