/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <getopt.h>
#include <iostream>
#include <limits>

#include "ivf.hh"
#include "uncompressed_chunk.hh"
#include "frame.hh"
#include "decoder.hh"
#include "ivf_writer.hh"
#include "enc_state_serializer.hh"
#include "serializer.cc"

using namespace std;

void usage_error( const string & program_name )
{
  cerr << "Usage: " << program_name << " <ivf-in> <ivf-out> [<output-state>]" << endl
       << endl
       << "Please note that your IVF input *must* start with a keyframe." << endl
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

    string input_file( argv[ 1 ] );
    string output_file( argv[ 2 ] );
    string output_state = "";

    if ( argc == 4 ) {
      output_state = argv[ 3 ];
    }

    IVF ivf( input_file );
    IVFWriter ivf_writer( output_file, ivf.fourcc(), ivf.width(), ivf.height(),
                          ivf.frame_rate(), ivf.time_scale() );

    Decoder decoder = Decoder( ivf.width(), ivf.height() );

    if ( not decoder.minihash_match( ivf.expected_decoder_minihash() ) ) {
      throw Invalid( "Decoder state / IVF mismatch" );
    }

    for ( size_t i = 0; i < ivf.frame_count(); i++ ) {
      UncompressedChunk uch { ivf.frame( i ), ivf.width(), ivf.height(), false };

      if ( uch.key_frame() ) {
        KeyFrame frame = decoder.parse_frame<KeyFrame>( uch );
        ivf_writer.append_frame( frame.serialize( decoder.get_state().probability_tables ) );

        decoder.decode_frame( frame );
      }
      else {
        InterFrame frame = decoder.parse_frame<InterFrame>( uch );

        if ( i == ivf.frame_count() - 1 ) {
          frame.mutable_header().refresh_last = true;
          frame.mutable_header().refresh_golden_frame = true;
          frame.mutable_header().refresh_alternate_frame = true;
          frame.mutable_header().copy_buffer_to_golden.clear();
          frame.mutable_header().copy_buffer_to_alternate.clear();
        }

        ivf_writer.append_frame( frame.serialize( decoder.get_state().probability_tables ) );

        decoder.decode_frame( frame );
      }
    }

    if ( output_state.length() > 0 ) {
      EncoderStateSerializer odata;
      decoder.serialize( odata );
      odata.write( output_state );
    }

    return EXIT_SUCCESS;

  } catch ( const exception &  e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
