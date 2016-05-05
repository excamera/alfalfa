#include <getopt.h>

#include <fstream>
#include <algorithm>
#include <iostream>
#include <string>

#include "frame_input.hh"
#include "ivf_reader.hh"
#include "yuv4mpeg.hh"
#include "frame.hh"
#include "player.hh"
#include "vp8_raster.hh"
#include "decoder.hh"
#include "encoder.hh"
#include "macroblock.hh"
#include "ivf_writer.hh"
#include "display.hh"

using namespace std;

void usage_error( const string & program_name )
{
  cerr << "Usage: " << program_name << " [options] [-o <output>] <input>" << endl
       << endl
       << "Options:" << endl
       << " -o <arg>, --output=<arg>              Output file name (default: output.ivf)" << endl
       << " -s <arg>, --ssim=<arg>                SSIM for the output" << endl
       << " -i <arg>, --input-format=<arg>        Input file format" << endl
       << "                                         ivf (default), y4m" << endl;
}

int main( int argc, char *argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc < 2 ) {
      usage_error( argv[ 0 ] );
      return EXIT_FAILURE;
    }

    string output_file = "output.ivf";
    string input_format = "ivf";
    double ssim = 0.99;

    const option command_line_options[] = {
      { "output",       required_argument, nullptr, 'o' },
      { "input-format", required_argument, nullptr, 'i' },
      { "ssim",         required_argument, nullptr, 's' },
      { 0, 0, nullptr, 0 }
    };

    while ( true ) {
      const int opt = getopt_long( argc, argv, "o:i:s:", command_line_options, nullptr );

      if ( opt == -1 ) {
        break;
      }

      switch ( opt ) {
      case 'o':
        output_file = optarg;
        break;

      case 'i':
        input_format = optarg;
        break;

      case 's':
        ssim = stod( optarg );
        break;

      default:
        throw runtime_error( "getopt_long: unexpected return value." );
      }
    }

    if ( optind >= argc ) {
      usage_error( argv[ 0 ] );
    }

    string input_file = argv[ optind ];
    shared_ptr< FrameInput > input_reader;

    if ( input_format == "ivf" ) {
      if ( input_file == "-" ) {
        throw runtime_error( "not supported" );
      }
      else {
        input_reader = make_shared<IVFReader>( input_file );
      }
    }
    else if ( input_format == "y4m" ) {
      if ( input_file == "-" ) {
        input_reader = make_shared<YUV4MPEGReader>( FileDescriptor( STDIN_FILENO ) );
      }
      else {
        input_reader = make_shared<YUV4MPEGReader>( input_file );
      }
    }
    else {
      throw runtime_error( "unsupported input format" );
    }

    Encoder encoder( output_file, input_reader->display_width(), input_reader->display_height() );

    Optional<RasterHandle> raster = input_reader->get_next_frame();

    size_t frame_index = 0;

    while ( raster.initialized() ) {
      double result_ssim = encoder.encode_as_keyframe( raster.get(), ssim );

      cerr << "Frame #" << frame_index++ << ": ssim(" << result_ssim << ")" << endl;

      raster = input_reader->get_next_frame();
    }
  } catch ( const exception &  e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
