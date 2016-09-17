/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <getopt.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
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
#include "enc_state_serializer.hh"

using namespace std;

void usage_error( const string & program_name )
{
  cerr << "Usage: " << program_name << " [options] <input>" << endl
       << endl
       << "Options:" << endl
       << " -o <arg>, --output=<arg>              Output file name (default: output.ivf)" << endl
       << " -s <arg>, --ssim=<arg>                SSIM for the output" << endl
       << " -i <arg>, --input-format=<arg>        Input file format" << endl
       << "                                         ivf (default), y4m" << endl
       << " -O <arg>, --output-state=<arg>        Output file name for final" << endl
       << "                                         encoder state (default: none)" << endl
       << " -I <arg>, --input-state=<arg>         Input file name for initial" << endl
       << "                                         encoder state (default: none)" << endl
       << " --two-pass                            Do the second encoding pass" << endl
       << " -y, --y-ac-qi <arg>                   Quantization index for Y" << endl
       << endl
       << "Re-encode:" << endl
       << " -r, --reencode                        Re-encode." << endl
       << " -p, --pred-ivf <arg>                  Prediction modes IVF" << endl
       << " -S, --pred-state <arg>                Prediction modes IVF initial state" << endl
       << " -w, --kf-q-weight <arg>               Keyframe quantizer weight" << endl
       << endl;
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
    string input_state = "";
    string output_state = "";
    string pred_file = "";
    string pred_ivf_initial_state = "";
    double ssim = 0.99;
    bool two_pass = false;
    bool re_encode_only = false;
    double kf_q_weight = 1.0;

    uint8_t y_ac_qi = numeric_limits<uint8_t>::max();

    const option command_line_options[] = {
      { "output",               required_argument, nullptr, 'o' },
      { "ssim",                 required_argument, nullptr, 's' },
      { "input-format",         required_argument, nullptr, 'i' },
      { "output-state",         required_argument, nullptr, 'O' },
      { "input-state",          required_argument, nullptr, 'I' },
      { "two-pass",             no_argument,       nullptr, '2' },
      { "y-ac-qi",              required_argument, nullptr, 'y' },
      { "reencode",             no_argument,       nullptr, 'r' },
      { "pred-ivf",             required_argument, nullptr, 'p' },
      { "pred-state",           required_argument, nullptr, 'S' },
      { "kf-q-weight",          required_argument, nullptr, 'w' },
      { 0, 0, 0, 0 }
    };

    while ( true ) {
      const int opt = getopt_long( argc, argv, "o:s:i:O:I:2y:p:S:rw:", command_line_options, nullptr );

      if ( opt == -1 ) {
        break;
      }

      switch ( opt ) {
      case 'o':
        output_file = optarg;
        break;

      case 's':
        ssim = stod( optarg );
        break;

      case 'i':
        input_format = optarg;
        break;

      case 'O':
        output_state = optarg;
        break;

      case 'I':
        input_state = optarg;
        break;

      case '2':
        two_pass = true;
        break;

      case 'y':
        y_ac_qi = stoul( optarg );
        break;

      case 'r':
        re_encode_only = true;
        break;

      case 'p':
        pred_file = optarg;
        break;

      case 'S':
        pred_ivf_initial_state = optarg;
        break;

      case 'w':
        kf_q_weight = stod( optarg );
        break;

      default:
        throw runtime_error( "getopt_long: unexpected return value." );
      }
    }

    if ( optind >= argc ) {
      usage_error( argv[ 0 ] );
      return EXIT_FAILURE;
    }

    string input_file = argv[ optind ];
    shared_ptr<FrameInput> input_reader;

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

    Decoder pred_decoder( input_reader->display_width(), input_reader->display_height() );

    if ( pred_file != "" and pred_ivf_initial_state != "") {
      pred_decoder = EncoderStateDeserializer::build<Decoder>( pred_ivf_initial_state );
    }

    IVFWriter output { output_file, "VP80", input_reader->display_width(), input_reader->display_height(), 1, 1 };

    if ( re_encode_only ) {
      /* re-encoding */
      if ( input_state.empty() ) {
        throw runtime_error( "re-encoding without an input_state" );
      }

      /* pre-read all the original rasters */
      vector<RasterHandle> original_rasters;
      while ( true ) {
        auto next_raster = input_reader->get_next_frame();
        if ( next_raster.initialized() ) {
          original_rasters.emplace_back( next_raster.get() );
        } else {
          break;
        }
      }

      /* pre-read all the prediction frames */
      vector<pair<Optional<KeyFrame>, Optional<InterFrame> > > prediction_frames;

      IVF pred_ivf { pred_file };

      if ( not pred_decoder.minihash_match( pred_ivf.expected_decoder_minihash() ) ) {
        throw Invalid( "Mismatch between prediction IVF and prediction_ivf_initial_state" );
      }

      for ( unsigned int i = 0; i < pred_ivf.frame_count(); i++ ) {
        UncompressedChunk unch { pred_ivf.frame( i ), pred_ivf.width(), pred_ivf.height() };

        if ( unch.key_frame() ) {
          KeyFrame frame = pred_decoder.parse_frame<KeyFrame>( unch );
          pred_decoder.decode_frame( frame );

          prediction_frames.emplace_back( move( frame ), Optional<InterFrame>() );
        } else {
          InterFrame frame = pred_decoder.parse_frame<InterFrame>( unch );
          pred_decoder.decode_frame( frame );

          prediction_frames.emplace_back( Optional<KeyFrame>(), move( frame ) );
        }
      }

      /* wait for EOF on stdin */
      FileDescriptor stdin( STDIN_FILENO );
      while ( not stdin.eof() ) {
        stdin.read( 1 );
      }

      Encoder encoder( EncoderStateDeserializer::build<Decoder>( input_state ),
                       move( output ), two_pass );

      encoder.set_expected_decoder_entry_hash( encoder.export_decoder().get_hash().hash() );

      encoder.reencode( original_rasters, prediction_frames, kf_q_weight );

      if (output_state != "") {
        EncoderStateSerializer odata = {};
        encoder.export_decoder().serialize(odata);
        odata.write(output_state);
      }
    }
    else {
      /* primary encoding */
      Encoder encoder = input_state == ""
        ? Encoder(move(output), two_pass)
        : Encoder( EncoderStateDeserializer::build<Decoder>( input_state ),
                   move( output ), two_pass );

      if ( not input_state.empty() ) {
        output.set_expected_decoder_entry_hash( encoder.export_decoder().get_hash().hash() );
      }

      Optional<RasterHandle> raster = input_reader->get_next_frame();

      size_t frame_index = 0;

      while ( raster.initialized() ) {
        double result_ssim = encoder.encode( raster.get(), ssim, y_ac_qi );

        cerr << "Frame #" << frame_index++ << ": ssim=" << result_ssim << endl;

        raster = input_reader->get_next_frame();
      }

      if ( not output_state.empty() ) {
        throw runtime_error( "unsupported: primary encode with output state" );
      }
    }
  } catch ( const exception &  e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
