#include <getopt.h>
#include <iostream>
#include <array>
#include <string>

#include "frame.hh"
#include "decoder.hh"
#include "ivf.hh"

using namespace std;

void usage_error( const string & program_name )
{
  cerr << "Usage: " << program_name << " [options] <ivf>" << endl
       << endl
       << "Options:" << endl
       << " -p, --probability-tables       Print the prob tables for each frame" << endl
       << " -c, --coefficients             Print DCT coefficients for each block" << endl
       << endl;
}

template <class Frame>
void print_probability_tables( const Frame & frame, const DecoderState & state )
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
            cout << ( int )state.probability_tables.coeff_probs.at( i ).at( j ).at( k ).at( l ) << "\t";
          }
        }

        cout << " }" << endl;
      }
    }
  }
}

static array<string, 5> mbmode_names =
  { "DC_PRED", "V_PRED", "H_PRED", "TM_PRED", "B_PRED" };

static array<string, 10> bmode_names =
  { "B_DC_PRED", "B_TM_PRED", "B_VE_PRED", "B_HE_PRED", "B_LD_PRED",
    "B_RD_PRED", "B_VR_PRED", "B_VL_PRED", "B_HD_PRED", "B_HU_PRED" };

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

  const option command_line_options[] = {
    { "probability-tables",        no_argument, nullptr, 'p' },
    { "coefficients",              no_argument, nullptr, 'c' },
    { 0, 0, nullptr, 0 }
  };

  while ( true ) {
    const int opt = getopt_long( argc, argv, "pc", command_line_options, nullptr );

    if ( opt == -1 ) {
      break;
    }

    switch ( opt ) {
    case 'p':
      probability_tables = true;
      break;

    case 'c':
      coefficients = true;
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

  DecoderState decoder_state { width, height };

  for ( size_t frame_number = 0; frame_number < ivf.frame_count(); frame_number++ ) {
    cout << "Frame #" << frame_number << endl;

    UncompressedChunk uncompressed_chunk { ivf.frame( frame_number ), width, height };

    if ( not uncompressed_chunk.key_frame() ) {
      cout << "Interframe (not implemented)." << endl;
      cout << endl;
      continue;
    }

    decoder_state = DecoderState{ width, height }; // reset the decoder state for keyframe
    KeyFrame frame = decoder_state.parse_and_apply<KeyFrame>( uncompressed_chunk );

    if ( probability_tables ) {
      cout << "[Probability Tables]" << endl;
      print_probability_tables( frame, decoder_state );
      cout << endl;
    }

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
    cout << "[Macroblocks]" << endl;

    frame.macroblocks().forall_ij(
      [&] ( KeyFrameMacroblock & macroblock, unsigned int mb_column, unsigned int mb_row )
      {
        cout << "Macroblock [ " << mb_column << ", " << mb_row << " ]" << endl;
        cout << "<Y>" << endl;
        cout << "Prediction Mode: " << mbmode_names[ macroblock.y_prediction_mode() ] << endl;
        cout << endl;

        macroblock.Y().forall_ij(
          [&] ( const YBlock & yblock, unsigned int sb_column, unsigned sb_row ) {
            cout << "Y Subblock [ " << sb_column << ", " << sb_row << " ]" << endl;
            cout << "Prediction Mode: " << bmode_names[ yblock.prediction_mode() ] << endl;

            if ( coefficients ) {
              cout << "DCT coeffs: { " << yblock.coefficients() << " }" << endl;
              cout << endl;
            }
          }
        );

        if ( macroblock.Y2().coded() ) {
          cout << "<Y2>" << endl;
          cout << "DCT coeffs: { " << macroblock.Y2().coefficients() << " }" << endl;
          cout << endl;
        }

        cout << endl;

        cout << "<U>" << endl;
        cout << "Prediction Mode: " << mbmode_names[ macroblock.uv_prediction_mode() ] << endl;
        cout << endl;

        macroblock.U().forall_ij(
          [&] ( const UVBlock & ublock, unsigned int sb_column, unsigned sb_row ) {
            cout << "U Subblock [ " << sb_column << ", " << sb_row << " ]" << endl;

            if ( coefficients ) {
              cout << "DCT coeffs: { " << ublock.coefficients() << " }" << endl;
              cout << endl;
            }
          }
        );

        cout << endl;

        cout << "<V>" << endl;
        cout << "Prediction Mode: " << mbmode_names[ macroblock.uv_prediction_mode() ] << endl;
        cout << endl;

        macroblock.V().forall_ij(
          [&] ( const UVBlock & vblock, unsigned int sb_column, unsigned sb_row ) {
            cout << "V Subblock [ " << sb_column << ", " << sb_row << " ]" << endl;

            if ( coefficients ) {
              cout << "DCT coeffs: { " << vblock.coefficients() << " }" << endl;
              cout << endl;
            }
          }
        );

        cout << endl;
      }
    );

    if ( coefficients ) {}

    cout << endl;
  }

  return EXIT_SUCCESS;
}
