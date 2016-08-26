/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <getopt.h>

#include <iostream>
#include <string>
#include "player.hh"

#include "decoder.hh"
#include "encoder.hh"
#include "ivf.hh"

using namespace std;

void usage_error( const string & program_name )
{
  cerr << "Usage: " << program_name << " [-o <output>] <input1> <input2>" << endl
       << endl;
       /* << "Options:" << endl
       << " -o <arg>, --output=<arg>              Output file name (default: output.ivf)" << endl
       << endl; */
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

    const option command_line_options[] = {
      { "output",       required_argument, nullptr, 'o' },
      { 0, 0, 0, 0 }
    };

    while ( true ) {
      const int opt = getopt_long( argc, argv, "o:s:", command_line_options, nullptr );

      if ( opt == -1 ) {
        break;
      }

      switch ( opt ) {
      case 'o':
        output_file = optarg;
        break;

      default:
        throw runtime_error( "getopt_long: unexpected return value." );
      }
    }

    if ( optind + 1 >= argc ) {
      usage_error( argv[ 0 ] );
      return EXIT_FAILURE;
    }

    string input_file_1( argv[ optind++ ] );
    string input_file_2( argv[ optind++ ] );

    FilePlayer player_1( input_file_1 );

    while ( not player_1.eof() ) {
      player_1.advance();
    }

    // final state of the first sequence
    References final_references = player_1.current_references();
    DecoderState final_state = player_1.current_state();

    const IVF ivf_1( input_file_1 );
    const IVF ivf_2( input_file_2 );
    Decoder decoder_2( ivf_2.width(), ivf_2.height() );

    // TODO check the dimensions

    UncompressedChunk uncompressed_chunk { ivf_2.frame( 0 ), ivf_2.width(), ivf_2.height() };
    assert( uncompressed_chunk.key_frame() );

    MutableRasterHandle unfiltered_output{ ivf_2.width(), ivf_2.height() };

    KeyFrame first_frame = decoder_2.parse_frame<KeyFrame>( uncompressed_chunk );
    first_frame.decode( decoder_2.get_state().segmentation, decoder_2.get_references(), unfiltered_output );

    Encoder encoder( Decoder( final_state, final_references ), output_file, false );

    for ( size_t i = 0; i < ivf_1.frame_count(); i++ ) {
      encoder.ivf_writer().append_frame( ivf_1.frame( i ) );
    }

    encoder.reencode_as_interframe( unfiltered_output, first_frame );

    for ( size_t i = 1; i < ivf_2.frame_count(); i++ ) {
      encoder.ivf_writer().append_frame( ivf_2.frame( i ) );
    }

    cerr << "Done." << endl;
  } catch ( const exception &  e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }


  return EXIT_SUCCESS;
}
