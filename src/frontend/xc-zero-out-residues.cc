/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Copyright 2013-2018 the Alfalfa authors
                       and the Massachusetts Institute of Technology

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

      1. Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

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
  cerr << "Usage: " << program_name << " <ivf-in> <ivf-out>" << endl;
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
        auto old_prob_tables = decoder.get_state().probability_tables;
        decoder.decode_frame( frame );

        frame.mutable_macroblocks().forall(
          []( InterFrameMacroblock & frame_mb )
          {
            frame_mb.zero_out();
          }
        );

        ivf_writer.append_frame( frame.serialize( old_prob_tables ) );
      }
    }

    return EXIT_SUCCESS;

  } catch ( const exception &  e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
