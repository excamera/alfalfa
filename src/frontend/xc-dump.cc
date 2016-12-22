/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <getopt.h>
#include <iostream>
#include <limits>

#include "ivf.hh"
#include "uncompressed_chunk.hh"
#include "frame.hh"
#include "decoder.hh"
#include "enc_state_serializer.hh"

using namespace std;

void usage_error( const string & program_name )
{
  cerr << "Usage: " << program_name << " [options] <ivf> <output>" << endl
       << endl
       << "Options:" << endl
       << " -f <arg>, --frame-number=<arg>        Frame number" << endl
       << "                                         defaults to the last frame" << endl
       << " -S <arg>, --input-state <arg>         IVF input initial state" << endl
       << endl;
}


int main( int argc, char *argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc < 3 ) {
      usage_error( argv[ 0 ] );
      return EXIT_FAILURE;
    }

    string input_file = "";
    string input_state = "";
    size_t frame_number = numeric_limits<size_t>::max();
    string output_file = "";

    const option command_line_options[] = {
      { "frame-number",        required_argument, nullptr, 'f' },
      { "input-state",         required_argument, nullptr, 'S' },
      { 0, 0, nullptr, 0 }
    };

    while ( true ) {
      const int opt = getopt_long( argc, argv, "f:S:", command_line_options, nullptr );

      if ( opt == -1 ) {
        break;
      }

      switch ( opt ) {
      case 'f':
        frame_number = stoul( optarg );
        break;

      case 'S':
        input_state = optarg;
        break;

      default:
        throw runtime_error( "getopt_long: unexpected return value." );
      }
    }

    if ( optind + 1 >= argc ) {
      usage_error( argv[ 0 ] );
    }

    input_file = argv[ optind++ ];
    output_file = argv[ optind++ ];

    IVF ivf( input_file );

    Decoder decoder = ( input_state == "" )
                    ? Decoder( ivf.width(), ivf.height() )
                    : EncoderStateDeserializer::build<Decoder>( input_state );

    if ( not decoder.minihash_match( ivf.expected_decoder_minihash() ) ) {
      throw Invalid( "Decoder state / IVF mismatch" );
    }

    if ( frame_number == numeric_limits<size_t>::max() ) {
      frame_number = ivf.frame_count() - 1;
    }

    for ( size_t i = 0; i < ivf.frame_count(); i++ ) {
      UncompressedChunk uch { ivf.frame( i ), ivf.width(), ivf.height(), false };

      if ( uch.key_frame() ) {
        KeyFrame frame = decoder.parse_frame<KeyFrame>( uch );
        decoder.decode_frame( frame );
      }
      else {
        InterFrame frame = decoder.parse_frame<InterFrame>( uch );
        decoder.decode_frame( frame );
      }

      if ( i == frame_number ) {
        EncoderStateSerializer odata;
        decoder.serialize( odata );
        odata.write( output_file );
        return EXIT_SUCCESS;
      }
    }

    throw runtime_error( "invalid frame number" );

  } catch ( const exception &  e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
