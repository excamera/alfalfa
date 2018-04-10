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

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <chrono>

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
  cerr << "Usage: " << program_name << " [options] <input>"                                  << endl
                                                                                             << endl
       << "Options:"                                                                         << endl
       << " -o <arg>, --output=<arg>              Output file name (default: output.ivf)"    << endl
       << " -s <arg>, --ssim=<arg>                SSIM for the output"                       << endl
       << " -i <arg>, --input-format=<arg>        Input file format"                         << endl
       << "                                         ivf (default), y4m"                      << endl
       << " -O <arg>, --output-state=<arg>        Output file name for final"                << endl
       << "                                         encoder state (default: none)"           << endl
       << " -I <arg>, --input-state=<arg>         Input file name for initial"               << endl
       << "                                         encoder state (default: none)"           << endl
       << " -y, --y-ac-qi=<arg>                   Quantization index for Y"                  << endl
       << " -q, --quality=(best|rt)               Quality setting"                           << endl
       << "                                         best: best quality, slowest (default)"   << endl
       << "                                         rt:   real-time"                         << endl
       << " -F <arg>, --frame-sizes=<arg>         Target frame sizes file"                   << endl
       << "                                         Each line specifies the target size"     << endl
       << "                                         in bytes for the corresponding frame."   << endl
       << " --two-pass                            Do the second encoding pass"               << endl
                                                                                             << endl
       << "Re-encode:"                                                                       << endl
       << " -r, --reencode                        Re-encode"                                 << endl
       << " -p, --pred-ivf <arg>                  Prediction modes IVF"                      << endl
       << " -S, --pred-state <arg>                Prediction modes IVF initial state"        << endl
       << " -w, --kf-q-weight <arg>               Keyframe quantizer weight"                 << endl
       << " -e, --extra-frame-chunk               Prediction IVF starts with an extra frame" << endl
       << " -W, --no-no_wait                      Don't wait for STDIN to start reencoding"  << endl
       << endl;
}

size_t read_next_frame_size( istream & in )
{
  static size_t last_size = numeric_limits<size_t>::max();

  if ( not in.eof() ) {
    in >> last_size;
  }

  return last_size;
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
    string frame_sizes_file = "";
    double ssim = 0.99;
    bool two_pass = false;
    bool re_encode_only = false;
    double kf_q_weight = 1.0;
    bool extra_frame_chunk = false;
    bool no_wait = false;
    Optional<uint8_t> y_ac_qi;
    EncoderQuality quality = BEST_QUALITY;

    EncoderMode encoder_mode = MINIMUM_SSIM;


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
      { "extra-frame-chunk",    no_argument,       nullptr, 'e' },
      { "quality",              required_argument, nullptr, 'q' },
      { "frame-sizes",          required_argument, nullptr, 'F' },
      { "no-wait",              no_argument,       nullptr, 'W' },
      { 0, 0, 0, 0 }
    };

    while ( true ) {
      const int opt = getopt_long( argc, argv, "o:s:i:O:I:2y:p:S:rw:eq:F:W", command_line_options, nullptr );

      if ( opt == -1 ) {
        break;
      }

      switch ( opt ) {
      case 'o':
        output_file = optarg;
        break;

      case 's':
        ssim = stod( optarg );
        encoder_mode = MINIMUM_SSIM;
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
        encoder_mode = CONSTANT_QUANTIZER;
        break;

      case 'r':
        re_encode_only = true;
        encoder_mode = REENCODE;
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

      case 'W':
        no_wait = true;
        break;

      case 'e':
        extra_frame_chunk = true;
        break;

      case 'q':
        if ( strcmp( optarg, "rt" ) == 0 ) {
          quality = REALTIME_QUALITY;
        }

        break;

      case 'F':
        frame_sizes_file = optarg;
        encoder_mode = TARGET_FRAME_SIZE;
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
      for ( size_t i = 0; ; i++ ) {
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
        UncompressedChunk unch { pred_ivf.frame( i ), pred_ivf.width(), pred_ivf.height(), false };

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
      if ( not no_wait ) {
        FileDescriptor stdin( STDIN_FILENO );
        while ( not stdin.eof() ) {
          stdin.read( 1 );
        }
      }

      Encoder encoder( EncoderStateDeserializer::build<Decoder>( input_state ),
                       two_pass, quality );

      output.set_expected_decoder_entry_hash( encoder.export_decoder().get_hash().hash() );

      encoder.reencode( original_rasters, prediction_frames, kf_q_weight,
                        extra_frame_chunk, output );

      if (output_state != "") {
        EncoderStateSerializer odata = {};
        encoder.export_decoder().serialize(odata);
        odata.write(output_state);
      }
    }
    else {
      /* primary encoding */
      Encoder encoder = input_state == ""
        ? Encoder( input_reader->display_width(), input_reader->display_height(),
                   two_pass, quality )
        : Encoder( EncoderStateDeserializer::build<Decoder>( input_state ),
                   two_pass, quality );

      if ( not input_state.empty() ) {
        output.set_expected_decoder_entry_hash( encoder.export_decoder().get_hash().hash() );
      }

      ifstream frame_sizes_if;

      if ( encoder_mode == TARGET_FRAME_SIZE ) {
        if ( frame_sizes_file != "-" ) {
          frame_sizes_if.open( frame_sizes_file.c_str() );
        }
      }

      istream & frame_sizes = ( encoder_mode == TARGET_FRAME_SIZE and frame_sizes_file != "-" )
                              ? frame_sizes_if
                              : cin;

      unsigned int frame_no = 0;
      for ( auto raster = input_reader->get_next_frame(); raster.initialized();
            raster = input_reader->get_next_frame() ) {

        cerr << "Encoding frame #" << frame_no++ << "...";
        const auto encode_beginning = chrono::system_clock::now();

        switch ( encoder_mode ) {
        case MINIMUM_SSIM:
          output.append_frame( encoder.encode_with_minimum_ssim( raster.get(), ssim ) );
          break;

        case CONSTANT_QUANTIZER:
          cerr << " [estimated size=" << encoder.estimate_frame_size( raster.get(), y_ac_qi.get() ) << "] ";
          output.append_frame( encoder.encode_with_quantizer( raster.get(), y_ac_qi.get() ) );
          break;

        case TARGET_FRAME_SIZE:
        {
          size_t target_size = read_next_frame_size( frame_sizes );
          output.append_frame( encoder.encode_with_target_size( raster.get(), target_size ) );
          cerr << " [target_size=" << target_size << "] ";
          break;
        }

        default:
          throw Unsupported( "unsupported encoder mode." );
        }

        const auto encode_ending = chrono::system_clock::now();
        const int ms_elapsed = chrono::duration_cast<chrono::milliseconds>( encode_ending - encode_beginning ).count();
        cerr << "done (" << ms_elapsed << " ms)." << endl;
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
