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
#include <array>
#include <string>
#include <iomanip>
#include <sstream>

#include "modemv_data.hh"
#include "frame.hh"
#include "decoder.hh"
#include "ivf.hh"

using namespace std;

void usage_error( const string & program_name )
{
  cerr << "Usage: " << program_name << " [options] <ivf>" << endl
       << endl
       << "Options:" << endl
       << " -m, --macroblocks              Print per-macroblock information" << endl
       << " -p, --probability-tables       Print the prob tables for each frame" << endl
       << " -c, --coefficients             Print DCT coefficients for each block" << endl
       << " -f, --frame <arg>              Print information for frame #<arg>" << endl
       << " -s, --initial-state <arg>      IVF initial state" << endl
       << endl;
}

template <class FrameType>
void print_probability_tables( const FrameType & frame, const DecoderState & )
{
  for ( unsigned int i = 0; i < BLOCK_TYPES; i++ ) {
    for ( unsigned int j = 0; j < COEF_BANDS; j++ ) {
      for ( unsigned int k = 0; k < PREV_COEF_CONTEXTS; k++ ) {
        cout << "[ " << i << ", " << j << ", " << k << " ] = { ";

        for ( unsigned int l = 0; l < ENTROPY_NODES; l++ ) {
          auto & node = frame.header().token_prob_update.at( i ).at( j ).at( k ).at( l ).coeff_prob;

          if ( node.initialized() ) {
            cout << ( int )node.get() << "\t";
          }
          else {
            cout << "-" << "\t";
          }
        }

        cout << " }" << endl;
      }
    }
  }
}

static array<string, 10> mbmode_names =
  { "DC_PRED", "V_PRED", "H_PRED", "TM_PRED", "B_PRED",
    "NEARESTMV", "NEARMV", "ZEROMV", "NEWMV", "SPLITMV" };

static array<string, 14> bmode_names =
  { "B_DC_PRED", "B_TM_PRED", "B_VE_PRED", "B_HE_PRED", "B_LD_PRED",
    "B_RD_PRED", "B_VR_PRED", "B_VL_PRED", "B_HD_PRED", "B_HU_PRED",
    "LEFT4X4", "ABOVE4X4", "ZERO4X4", "NEW4X4" };

static array<string, 4> reference_frame_names =
  { "CURRENT_FRAME", "LAST_FRAME", "GOLDEN_FRAME", "ALTREF_FRAME" };

template<class KeyType>
void print_key( const KeyType & key, const size_t level = 0 )
{
  wstringstream ss;
  for ( size_t i = 0; i < level; i++ ) { ss << "  "; }
  //if ( level > 0 ) { ss << L"\342\224\224\342\224\200 "; }
  ss << key << ": ";

  wcout << setw( 25 ) << left << ss.str();
}

template<class KeyType, class ValueType>
void print_key_value( const KeyType & key, const ValueType & value, const size_t level = 0 )
{
  print_key( key, level );
  cout << value << endl;
}

template<class FrameHeaderType>
void print_header_common( const FrameHeaderType & header )
{
  print_key_value( "refresh_entropy_probs", header.refresh_entropy_probs );
  print_key_value( "update_segmentation", header.update_segmentation.initialized() );
  print_key_value( "filter_type",         header.filter_type );
  print_key_value( "mode_lf_adjustments", header.mode_lf_adjustments.initialized() );

  if (  header.mode_lf_adjustments.initialized() ) {
  auto mode_ref_lf_delta_update = header.mode_lf_adjustments.get();
  print_key_value( "lf_delta_update", mode_ref_lf_delta_update.initialized(), 1 );

    if ( mode_ref_lf_delta_update.initialized() ) {
      print_key( "ref_update", 2 );
      for ( size_t i = 0; i < 4; i++ ) {
        auto & ref_update = mode_ref_lf_delta_update.get().ref_update.at( i );

        cout << setw( 6 ) << right;
        if ( ref_update.initialized() ) {
          cout << (int)ref_update.get();
        }
        else {
          cout << "X";
        }
      }
      cout << endl;

      print_key( "mode_update", 2 );
      for ( size_t i = 0; i < 4; i++ ) {
        auto & mode_update = mode_ref_lf_delta_update.get().mode_update.at( i );

        cout << setw( 6 ) << right;
        if ( mode_update.initialized() ) {
          cout << (int)mode_update.get();
        }
        else {
          cout << "X";
        }
      }
      cout << endl;
    }
  }

  print_key_value( "loop_filter_level", (int)header.loop_filter_level );
  print_key_value( "sharpness_level", (int)header.sharpness_level );
  print_key_value( "mb_no_skip_coeff", header.prob_skip_false.initialized() );

  if ( header.prob_skip_false.initialized() ) {
    print_key_value( "prob_skip_false", (int)header.prob_skip_false.get() );
  }
}

void print_header( const KeyFrameHeader & header )
{
  cout << "[Keyframe Header]" << endl;
  print_key_value( "color_space", header.color_space );
  print_key_value( "clamping_type", header.clamping_type );

  print_header_common( header );

  cout << endl;
}

void print_header( const InterFrameHeader & header )
{
  cout << "[Interframe Header]" << endl;

  print_header_common( header );

  print_key_value( "refresh_last", header.refresh_last );
  print_key_value( "refresh_golden_frame", header.refresh_golden_frame );
  print_key_value( "refresh_alternate_frame", header.refresh_alternate_frame );

  print_key_value( "prob_inter", (int)header.prob_inter );
  print_key_value( "prob_last", (int)header.prob_references_last );
  print_key_value( "prob_golden", (int)header.prob_references_golden );
  print_key_value( "16x16_prob_update", header.intra_16x16_prob.initialized() );

  if ( header.intra_16x16_prob.initialized() ) {
    print_key( "16x16_prob", 1 );
    for ( size_t i = 0; i < 4; i++ ) {
      cout << setw( 6 ) << right << (int)header.intra_16x16_prob.get().at( i );
    }
    cout << endl;
  }

  print_key_value( "chroma_prob_update", header.intra_chroma_prob.initialized() );

  if( header.intra_chroma_prob.initialized() ) {
    print_key( "chroma_prob", 1 );
    for ( size_t i = 0; i < 3; i++ ) {
      cout << setw( 6 ) << right << (int)header.intra_chroma_prob.get().at( i );
    }
    cout << endl;
  }

  print_key( "mv_prob_update" );
  for ( size_t i = 0; i < 2; i++ ) {
    for ( size_t j = 0; j < 19; j++ ) {
      if ( header.mv_prob_update.at( i ).at ( j ).initialized() ) {
        cout << (int)header.mv_prob_update.at( i ).at ( j ).get();
      }
      else {
        cout << "-";
      }
      if ( i == 0 or j != 18 ) {
        cout << "|";
      }
    }
  }
  cout << endl;

  cout << endl;
}

template <class FrameHeaderType, class MacroblockType>
void print_frame_info( const Frame<FrameHeaderType, MacroblockType> & frame, const DecoderState & decoder_state,
                       const bool probability_tables, const bool macroblocks,
                       const bool coefficients )
{
  if ( probability_tables ) {
    cout << "[Probability Tables]" << endl;
    print_probability_tables( frame, decoder_state );
    cout << endl;
  }

  if ( not frame.show_frame() ) {
    cout << "(hidden frame)" << endl;
  }

  print_header( frame.header() );

  cout << "[Quantizer]" << endl;
  cout << "y_ac_qi = " << ( int )frame.header().quant_indices.y_ac_qi << endl;

  if ( frame.header().quant_indices.y_dc.initialized() ) {
    cout << "y_dc    = " << ( int )frame.header().quant_indices.y_dc.get() << endl;
  }

  if ( frame.header().quant_indices.y2_dc.initialized() ) {
    cout << "y2_dc   = " << ( int )frame.header().quant_indices.y2_dc.get() << endl;
  }

  if ( frame.header().quant_indices.y2_ac.initialized() ) {
    cout << "y2_ac   = " << ( int )frame.header().quant_indices.y2_ac.get() << endl;
  }

  if ( frame.header().quant_indices.uv_dc.initialized() ) {
    cout << "uv_dc   = " << ( int )frame.header().quant_indices.uv_dc.get() << endl;
  }

  if ( frame.header().quant_indices.uv_ac.initialized() ) {
    cout << "uv_ac   = " << ( int )frame.header().quant_indices.uv_ac.get() << endl;
  }

  cout << endl;

  if ( macroblocks ) {
    cout << "[Macroblocks]" << endl;

    frame.macroblocks().forall_ij(
      [&] ( MacroblockType & macroblock, unsigned int mb_column, unsigned int mb_row )
      {
        cout << "Macroblock [ " << mb_column << ", " << mb_row << " ]" << endl;
        cout << "<Y>" << endl;

        cout << "Prediction Mode: " << mbmode_names[ macroblock.y_prediction_mode() ] << endl;

        if ( macroblock.inter_coded() ) {
          cout << "Base Motion Vector: ( " << macroblock.base_motion_vector().x()
               << ", " << macroblock.base_motion_vector().y() << " )" << endl;
          cout << "Reference: " << reference_frame_names[ macroblock.header().reference() ] << endl;
        }

        cout << endl;

        macroblock.Y().forall_ij(
          [&] ( const YBlock & yblock, unsigned int sb_column, unsigned sb_row ) {
            cout << "Y Subblock [ " << sb_column << ", " << sb_row << " ]" << endl;

            if ( macroblock.y_prediction_mode() == B_PRED or
            macroblock.y_prediction_mode() == SPLITMV ) {
              cout << "Prediction Mode: " << bmode_names[ yblock.prediction_mode() ] << endl;
            }

            if ( coefficients and yblock.has_nonzero()  ) {
              cout << "DCT coeffs: { " << yblock.coefficients() << " }" << endl;
              cout << endl;
            }
            else if ( coefficients ) {
              cout << "ALL ZERO" << endl;
            }
          }
        );

        if ( coefficients and macroblock.Y2().coded() ) {
          cout << "<Y2>" << endl;

          if ( macroblock.Y2().has_nonzero() ) {
            cout << "DCT coeffs: { " << macroblock.Y2().coefficients() << " }" << endl;
          }
          else {
            cout << "ALL ZERO" << endl;
          }

          cout << endl;
        }

        cout << endl;

        cout << "<U>" << endl;
        if ( not macroblock.inter_coded() ) {
          cout << "Prediction Mode: " << mbmode_names[ macroblock.uv_prediction_mode() ] << endl;
        }
        cout << endl;

        macroblock.U().forall_ij(
          [&] ( const UVBlock & ublock, unsigned int sb_column, unsigned sb_row ) {
            cout << "U Subblock [ " << sb_column << ", " << sb_row << " ]" << endl;

            if ( coefficients and ublock.has_nonzero() ) {
              cout << "DCT coeffs: { " << ublock.coefficients() << " }" << endl;
              cout << endl;
            }
            else if ( coefficients ) {
              cout << "ALL ZERO" << endl;
            }
          }
        );

        cout << endl;

        cout << "<V>" << endl;
        if ( not macroblock.inter_coded() ) {
          cout << "Prediction Mode: " << mbmode_names[ macroblock.uv_prediction_mode() ] << endl;
        }
        cout << endl;

        macroblock.V().forall_ij(
          [&] ( const UVBlock & vblock, unsigned int sb_column, unsigned sb_row ) {
            cout << "V Subblock [ " << sb_column << ", " << sb_row << " ]" << endl;

            if ( coefficients and vblock.has_nonzero() ) {
              cout << "DCT coeffs: { " << vblock.coefficients() << " }" << endl;
              cout << endl;
            }
            else if ( coefficients ) {
              cout << "ALL ZERO" << endl;
            }
          }
        );

        cout << endl;
      }
    );
  } // end of macroblocks
}

int main( int argc, char *argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  if ( argc < 2 ) {
    usage_error( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  bool probability_tables = false;
  bool coefficients = false;
  bool macroblocks = false;
  size_t target_frame_number = SIZE_MAX;
  string input_state = "";

  const option command_line_options[] = {
    { "macroblocks",               no_argument, nullptr, 'm' },
    { "probability-tables",        no_argument, nullptr, 'p' },
    { "coefficients",              no_argument, nullptr, 'c' },
    { "frame",               required_argument, nullptr, 'f' },
    { "initial-state",       required_argument, nullptr, 's' },
    { 0, 0, nullptr, 0 }
  };

  while ( true ) {
    const int opt = getopt_long( argc, argv, "mpcf:s:", command_line_options, nullptr );

    if ( opt == -1 ) {
      break;
    }

    switch ( opt ) {
    case 'm':
      macroblocks = true;
      break;

    case 'p':
      probability_tables = true;
      break;

    case 'c':
      coefficients = true;
      break;

    case 'f':
      target_frame_number = stoul( optarg );
      break;

    case 's':
      input_state = optarg;
      break;

    default:
      throw runtime_error( "getopt_long: unexpected return value." );
    }
  }

  if ( optind >= argc ) {
    usage_error( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  string video_file = argv[ optind ];

  const IVF ivf( video_file );

  uint16_t width = ivf.width();
  uint16_t height = ivf.height();

  Decoder decoder = ( input_state == "" )
                  ? Decoder( ivf.width(), ivf.height() )
                  : EncoderStateDeserializer::build<Decoder>( input_state );

  if ( not decoder.minihash_match( ivf.expected_decoder_minihash() ) ) {
    cerr << "WARNING: Decoder state does not match IVF expected decoder state. Coefficients will be incorrect.\n";
  }

  DecoderState decoder_state = decoder.get_state();

  for ( size_t frame_number = 0; frame_number < ivf.frame_count(); frame_number++ ) {
    UncompressedChunk uncompressed_chunk { ivf.frame( frame_number ), width, height, false };

    if( target_frame_number == SIZE_MAX or frame_number == target_frame_number ) {
      cout << ">> Frame #" << frame_number << " "
           << ( uncompressed_chunk.key_frame() ? "(KF)" : "(IF)" ) << endl;
      cout << "   Size: " << ivf.frame( frame_number ).size() << " B" << endl;
    }

    if ( uncompressed_chunk.key_frame() ) {
      decoder_state = DecoderState{ width, height }; // reset the decoder state for the keyframe

      KeyFrame frame = decoder_state.parse_and_apply<KeyFrame>( uncompressed_chunk );

      if( target_frame_number == SIZE_MAX or frame_number == target_frame_number ) {
        print_frame_info( frame, decoder_state, probability_tables, macroblocks, coefficients );
      }
    }
    else {
      InterFrame frame = decoder_state.parse_and_apply<InterFrame>( uncompressed_chunk );

      if( target_frame_number == SIZE_MAX or frame_number == target_frame_number ) {
        print_frame_info( frame, decoder_state, probability_tables, macroblocks, coefficients );
      }
    }
  }

  return EXIT_SUCCESS;
}
