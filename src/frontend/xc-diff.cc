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
#include <array>
#include <iomanip>

#include "ivf.hh"
#include "frame.hh"
#include "decoder.hh"
#include "enc_state_serializer.hh"

using namespace std;

void usage_error( const string & program_name )
{
  cerr << "Usage: " << program_name << " [options] <state-1> <state-2>" << endl
       << endl
       << "Options:" << endl
       << endl;
}

void print_message( string message, size_t level )
{
  for ( size_t i = 0; i < level; i++ ) { cout << "  "; }
  cout << message << endl;
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

    const option command_line_options[] = {
      { 0, 0, nullptr, 0 }
    };

    while ( true ) {
      const int opt = getopt_long( argc, argv, "", command_line_options, nullptr );

      if ( opt == -1 ) {
        break;
      }

      switch( opt ) {
      default:
        throw runtime_error( "getopt_long: unexpected return value." );
      }
    }

    if ( optind + 1 >= argc ) {
      usage_error( argv[ 0 ] );
    }

    array<const string, 2> state_file = { argv[ optind ], argv[ optind + 1 ] };

    array<const Decoder, 2> decoder = {
      EncoderStateDeserializer::build<Decoder>( state_file[ 0 ] ),
      EncoderStateDeserializer::build<Decoder>( state_file[ 1 ] )
    };

    bool different = ( decoder[ 0 ] != decoder[ 1 ] );

    if ( not different ) {
      return EXIT_SUCCESS;
    }

    print_message( "Decoders are different.", 0 );

    if ( decoder[ 0 ].get_width() != decoder[ 1 ].get_width() or
         decoder[ 0 ].get_height() != decoder[ 1 ].get_height() ) {
      print_message( "Dimensions are different.", 1 );
    }

    if ( decoder[ 0 ].get_state() != decoder[ 1 ].get_state() ) {
      array<const DecoderState, 2> state = { decoder[ 0 ].get_state(),
                                             decoder[ 1 ].get_state() };

      print_message( "Decoder states are diffrent.", 1 );

      if ( state[ 0 ].probability_tables != state[ 1 ].probability_tables ) {
        print_message( "Probability tables are different.", 2 );

        if ( state[ 0 ].probability_tables.coeff_probs !=
             state[ 1 ].probability_tables.coeff_probs ) {
          print_message( "Coefficient probabilities are different.", 3 );

          // for ( unsigned int i = 0; i < BLOCK_TYPES; i++ ) {
          //   for ( unsigned int j = 0; j < COEF_BANDS; j++ ) {
          //     for ( unsigned int k = 0; k < PREV_COEF_CONTEXTS; k++ ) {
          //       for ( unsigned int l = 0; l < ENTROPY_NODES; l++ ) {
          //         if ( state[ 0 ].probability_tables.coeff_probs.at( i ).at( j ).at( k ).at( l ) !=
          //              state[ 1 ].probability_tables.coeff_probs.at( i ).at( j ).at( k ).at( l ) ) {
          //           cout << i << ", " << j << ", " << k << ", " << l << endl;
          //         }
          //       }
          //     }
          //   }
          // }
        }

        if ( state[ 0 ].probability_tables.y_mode_probs !=
             state[ 1 ].probability_tables.y_mode_probs ) {
          print_message( "Y-mode probabilities are different.", 3 );
        }

        if ( state[ 0 ].probability_tables.uv_mode_probs !=
             state[ 1 ].probability_tables.uv_mode_probs ) {
          print_message( "UV-mode probabilities are different.", 3 );
        }

        if ( state[ 0 ].probability_tables.motion_vector_probs !=
             state[ 1 ].probability_tables.motion_vector_probs ) {
          print_message( "Motion vector probabilities are different.", 3 );
        }
      }

      if ( state[ 0 ].segmentation != state[ 1 ].segmentation ) {
        print_message( "Segmentations are different.", 2 );
      }

      if ( state[ 0 ].filter_adjustments != state[ 1 ].filter_adjustments ) {
        print_message( "Filter adjustments are different.", 2 );
      }
    }

    if ( decoder[ 0 ].get_references() != decoder[ 1 ].get_references() ) {
      print_message( "References are different.", 1 );

      if ( decoder[ 0 ].get_references().last != decoder[ 1 ].get_references().last ) {
        print_message( "Last references are different.", 2 );
      }

      if ( decoder[ 0 ].get_references().golden != decoder[ 1 ].get_references().golden ) {
        print_message( "Golden references are different.", 2 );
      }

      if ( decoder[ 0 ].get_references().alternative != decoder[ 1 ].get_references().alternative ) {
        print_message( "Alternative references are different.", 2 );
      }
    }

    return different;
  } catch ( const exception &  e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
