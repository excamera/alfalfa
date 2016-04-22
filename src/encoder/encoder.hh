#ifndef ENCODER_HH
#define ENCODER_HH

#include <fstream>
#include <cmath>

#include "transform_sse.hh"
#include "dct.hh"
#include "block.hh"
#include "vp8_raster.hh"
#include "frame.hh"

using namespace std;

/* Temporary class, things will be moved around as soon as I realize what the fuck I'm doing. */
class Encoder
{
public:
  DCTCoefficients get_dct_coeffs( const VP8Raster::Block4 & block, uint8_t bias )
  {
    int pitch = 8;
    short dct_input[ 16 ];
    short dct_output[ 16 ];

    //cout << "DCT Bias: " << bias << endl;

    for ( size_t i = 0; i < 4; i++ ) {
      for ( size_t j = 0; j < 4; j++ ) {
        dct_input[ i + 4 * j ] = block.at( i, j ) - bias;
      }
    }

    cout << "input: ";
    for ( int i = 0; i < 16; i++ ) {
      cout << dct_input[ i ] <<  " ";
    }
    cout << endl;

    vp8_short_fdct4x4_c( dct_input, dct_output, pitch );

    DCTCoefficients dct_coeffs;

    for ( size_t i = 0; i < 16; i++ ) {
      dct_coeffs.at( i ) = dct_output[ i ];
    }

    cout << "output: " << dct_coeffs << endl;

    return dct_coeffs;
  }

  template<class BlockType>
  void quantize( uint16_t dc_factor, uint16_t ac_factor, const DCTCoefficients & dct_coeffs, BlockType & output )
  {
    output.mutable_coefficients().at( 0 ) =  dct_coeffs.at( 0 ) / dc_factor;

    for ( size_t i = 1; i < 16; i++ ) {
      output.mutable_coefficients().at( i ) =  dct_coeffs.at( i ) / ac_factor;
    }
  }

  static KeyFrame make_empty_frame( unsigned int width, unsigned int height ) {
    BoolDecoder data { { nullptr, 0 } };
    KeyFrame frame { true, width, height, data };
    frame.parse_macroblock_headers( data, ProbabilityTables {} );
    return frame;
  }

  void encode( const VP8Raster & raster, KeyFrame & original_frame )
  {
    //KeyFrame frame = make_empty_frame( raster.display_width(), raster.display_height() );
    KeyFrame & frame = original_frame;
    Quantizer quantizer( frame.header().quant_indices );

    raster.macroblocks().forall_ij( [&]( const VP8Raster::Macroblock & macroblock,
      unsigned int mb_column,
      unsigned int mb_row ) {
        cout << "Processing Macroblock: " << mb_column << ":" << mb_row << endl;

        auto & frame_macroblock = frame.mutable_macroblocks().at( mb_column, mb_row );
        auto & mb_y = macroblock.Y;

        uint8_t prediction_value = 0;

        if ( mb_y.context().above.initialized() and mb_y.context().left.initialized() ) {
          static constexpr uint8_t log2size = 4;

          prediction_value = ((mb_y.predictors().above_row.sum(int16_t())
        		    + mb_y.predictors().left_column.sum(int16_t())) + (1 << log2size))
        		  >> (log2size+1);
        }
        else {
          prediction_value = 128;

          static constexpr uint8_t log2size = 4;
          if ( mb_y.context().above.initialized() ) {
            prediction_value = (mb_y.predictors().above_row.sum(int16_t()) + (1 << (log2size-1))) >> log2size;
          } else if ( mb_y.context().left.initialized() ) {
            prediction_value = (mb_y.predictors().left_column.sum(int16_t()) + (1 << (log2size-1))) >> log2size;
          }
        }

        cout << "Original prediction mode: " << original_frame.mutable_macroblocks().at( mb_column, mb_row ).Y2_.prediction_mode() << endl;
        cout << "Bias: " << (int)prediction_value << endl;

        short walsh_input[ 16 ];
        short walsh_output[ 16 ];

        /* Y */
        macroblock.Y_sub.forall_ij( [&]( const VP8Raster::Block4 & subblock,
          unsigned int sb_column,
          unsigned int sb_row ) {
            auto & frame_subblock = frame_macroblock.Y_.at( sb_column, sb_row );
            cout << "current: " << frame_subblock.coefficients() << endl;
            auto dct_coeffs = get_dct_coeffs( subblock, prediction_value );

            walsh_input[ sb_column * 4 + sb_row ] = dct_coeffs.at( 0 );
            dct_coeffs.at( 0 ) = 0;

            quantize( quantizer.y_dc, quantizer.y_ac, dct_coeffs, frame_subblock );
            cout << "quantized: " << frame_subblock.coefficients() << endl;
            frame_subblock.set_prediction_mode( B_DC_PRED );
          } );

        /* Y2 */
        vp8_short_walsh4x4_c( walsh_input, walsh_output, 4 );

        walsh_output[ 0 ] /= quantizer.y2_dc;
        for ( int i = 1; i < 16; i++ ) {
          walsh_output[ i ] /= quantizer.y2_ac;
        }
        cout << "current prediction mode: " << frame_macroblock.Y2_.prediction_mode() << endl;
        cout << "current walsh: ";
        for ( size_t i = 0; i < 16; i++ ) {
          cout << frame_macroblock.Y2_.mutable_coefficients().at( i ) << " ";
        }

        frame_macroblock.Y2_.set_prediction_mode( DC_PRED );

        cout << endl;

        cout << "input walsh: ";
        for ( size_t i = 0; i < 16; i++ ) {
          cout << walsh_input[ i ] << " ";
        }
        cout << endl;

        cout << "output walsh: ";
        for ( size_t i = 0; i < 4; i++ ) {
          for ( size_t j = 0; j < 4; j++ ) {
            cout << walsh_output[ i * 4 + j] << " ";
            frame_macroblock.Y2_.mutable_coefficients().at( i + 4 * j ) = walsh_output[ i + 4 * j ];
          }
        }
        cout << endl;

        return;

        /* U */
        // frame_macroblock.U_.at(0, 0).set_prediction_mode( DC_PRED );
        auto & mb_u = macroblock.U;

        prediction_value = 0;

        if ( mb_u.context().above.initialized() and mb_u.context().left.initialized() ) {
          static constexpr uint8_t log2size = 3;

          prediction_value = ((mb_u.predictors().above_row.sum(int16_t())
        		    + mb_u.predictors().left_column.sum(int16_t())) + (1 << log2size))
        		  >> (log2size+1);
        }
        else {
          prediction_value = 128;
          static constexpr uint8_t log2size = 3;

          if ( mb_u.context().above.initialized() ) {
            prediction_value = (mb_u.predictors().above_row.sum(int16_t()) + (1 << (log2size-1))) >> log2size;
          } else if ( mb_u.context().left.initialized() ) {
            prediction_value = (mb_u.predictors().left_column.sum(int16_t()) + (1 << (log2size-1))) >> log2size;
          }
        }
        cout << "U prediciton: " << (int)prediction_value << endl;

        macroblock.U_sub.forall_ij( [&]( const VP8Raster::Block4 & subblock,
          unsigned int sb_column,
          unsigned int sb_row ) {
            return;
            cout << frame_macroblock.U_.at(sb_column, sb_row).coefficients() << endl;
            auto dct_coeffs = get_dct_coeffs( subblock, prediction_value );
            auto & frame_subblock = frame_macroblock.U_.at( sb_column, sb_row );
            quantize( quantizer.uv_dc, quantizer.uv_ac, dct_coeffs, frame_subblock );
          } );

        /* V */
        auto & mb_v = macroblock.V;
        prediction_value = 0;

        if ( mb_v.context().above.initialized() and mb_v.context().left.initialized() ) {
          static constexpr uint8_t log2size = 3;

          prediction_value = ((mb_v.predictors().above_row.sum(int16_t())
        		    + mb_v.predictors().left_column.sum(int16_t())) + (1 << log2size))
        		  >> (log2size+1);
        }
        else {
          prediction_value = 128;
          static constexpr uint8_t log2size = 3;

          if ( mb_v.context().above.initialized() ) {
            prediction_value = (mb_v.predictors().above_row.sum(int16_t()) + (1 << (log2size-1))) >> log2size;
          } else if ( mb_v.context().left.initialized() ) {
            prediction_value = (mb_v.predictors().left_column.sum(int16_t()) + (1 << (log2size-1))) >> log2size;
          }
        }

        macroblock.V_sub.forall_ij( [&]( const VP8Raster::Block4 & subblock,
          unsigned int sb_column,
          unsigned int sb_row ) {
            return;
            auto dct_coeffs = get_dct_coeffs( subblock, prediction_value );
            auto & frame_subblock = frame_macroblock.V_.at( sb_column, sb_row );
            quantize( quantizer.uv_dc, quantizer.uv_ac, dct_coeffs, frame_subblock );
          } );
      } );

    DecoderState decoder_state( raster.display_width(), raster.display_height() );

    ofstream fout("output.vp8");

    /* write IVF header */
    fout << "DKIF";
    fout << uint8_t(0) << uint8_t(0); /* version */
    fout << uint8_t(32) << uint8_t(0); /* header length */
    fout << "VP80"; /* fourcc */
    fout << uint8_t(raster.display_width() & 0xff) << uint8_t((raster.display_width() >> 8) & 0xff); /* width */
    fout << uint8_t(raster.display_height() & 0xff) << uint8_t((raster.display_height() >> 8) & 0xff); /* height */
    fout << uint8_t(1) << uint8_t(0) << uint8_t(0) << uint8_t(0); /* bogus frame rate */
    fout << uint8_t(1) << uint8_t(0) << uint8_t(0) << uint8_t(0); /* bogus time scale */

    const uint32_t le_num_frames = htole32( 1 );
    fout << string( reinterpret_cast<const char *>( &le_num_frames ), sizeof( le_num_frames ) ); /* num frames */
    fout << uint8_t(0) << uint8_t(0) << uint8_t(0) << uint8_t(0); /* fill out header */

    vector< uint8_t > serialized_frame = frame.serialize( ProbabilityTables{} );

    const uint32_t le_size = htole32( serialized_frame.size() );
    fout << string( reinterpret_cast<const char *>( &le_size ), sizeof( le_size ) ); /* size */

    fout << uint8_t(0) << uint8_t(0) << uint8_t(0) << uint8_t(0); /* fill out header */
    fout << uint8_t(0) << uint8_t(0) << uint8_t(0) << uint8_t(0); /* fill out header */

    /* write the frame */
    fout.write( (const char*)serialized_frame.data(), serialized_frame.size() );
    fout.close();

    return;
  }
};

#endif /* ENCODER_HH */
