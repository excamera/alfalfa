#include "encoder.hh"

using namespace std;

KeyFrame Encoder::make_empty_frame( unsigned int width, unsigned int height )
{
  BoolDecoder data { { nullptr, 0 } };
  KeyFrame frame { true, width, height, data };
  frame.parse_macroblock_headers( data, ProbabilityTables {} );
  return frame;
}

template <class FrameHeaderType, class MacroblockHeaderType>
SafeArray< int16_t, 16 > Macroblock<FrameHeaderType, MacroblockHeaderType>::extract_y_dc_coeffs( bool set_to_zero ) {
  SafeArray< int16_t, 16 > result;

  for ( size_t column = 0; column < 4; column++ ) {
    for ( size_t row = 0; row < 4; row++ ) {
      result.at( column * 4 + row ) = Y_.at( column, row ).coefficients().at( 0 );

      if ( set_to_zero ) {
        Y_.at( column, row ).set_dc_coefficient( 0 );
      }
    }
  }

  return result;
}

template <unsigned int size>
template <class PredictionMode>
void VP8Raster::Block<size>::intra_predict( const PredictionMode mb_mode, TwoD< uint8_t > & output )
{
  TwoDSubRange< uint8_t, size, size > subrange( output, 0, 0 );
  intra_predict( mb_mode, subrange );
}

/* Encoder */
Encoder::Encoder( const string & output_filename, uint16_t width, uint16_t height )
  : ivf_writer_( output_filename, "VP80", width, height, 1, 1 )
{}

void Encoder::encode_as_keyframe( const VP8Raster & raster )
{
  unsigned int width = raster.display_width();
  unsigned int height = raster.display_height();

  KeyFrame frame = Encoder::make_empty_frame( width, height );
  DecoderState temp_state { width, height };
  Quantizer quantizer( frame.header().quant_indices );

  raster.macroblocks().forall_ij( [&] ( VP8Raster::Macroblock & raster_macroblock,
    unsigned int column_mb, unsigned int row_mb ) {
      auto & frame_macroblock = frame.mutable_macroblocks().at( column_mb, row_mb );
      // Process Y
      frame_macroblock.Y2().set_prediction_mode( B_PRED ); // predict each subblock separately

      raster_macroblock.Y_sub.forall_ij( [&] ( VP8Raster::Block4 & raster_y_subblock,
        unsigned int column_sb, unsigned int row_sb ) {
          YBlock & frame_y_subblock = frame_macroblock.Y().at( column_sb, row_sb );

          TwoD< uint8_t > prediction( 4, 4 );
          raster_y_subblock.intra_predict( B_DC_PRED, prediction );
          frame_y_subblock.set_prediction_mode( B_DC_PRED );
          frame_y_subblock.mutable_coefficients().subtract_dct( raster_y_subblock,
                                                                TwoDSubRange< uint8_t, 4, 4 >( prediction, 0, 0 ) );
          frame_y_subblock.mutable_coefficients() = YBlock::quantize( quantizer, frame_y_subblock.coefficients() );

          frame_y_subblock.set_Y_without_Y2();
          frame_y_subblock.calculate_has_nonzero();
        } );

        // Process Y2
        // Not needed for now
        frame_macroblock.Y2().set_coded( false );

        // Process U
        frame_macroblock.U().at( 0, 0 ).set_prediction_mode( DC_PRED );
        TwoD< uint8_t > prediction( 8, 8 );

        raster_macroblock.U.intra_predict( DC_PRED, prediction );
        raster_macroblock.U_sub.forall_ij( [&] ( VP8Raster::Block4 & raster_u_subblock,
          unsigned int column_sb, unsigned int row_sb ) {
            UVBlock & frame_u_subblock = frame_macroblock.U().at( column_sb, row_sb );

            frame_u_subblock.mutable_coefficients().subtract_dct( raster_u_subblock,
                                                                  TwoDSubRange< uint8_t, 4, 4 >( prediction, 4 * column_sb, 4 * row_sb ) );
            frame_u_subblock.mutable_coefficients() = UVBlock::quantize( quantizer, frame_u_subblock.coefficients() );
            frame_u_subblock.calculate_has_nonzero();
          } );

          // Process V
          raster_macroblock.V.intra_predict( DC_PRED, prediction );
          raster_macroblock.V_sub.forall_ij( [&] ( VP8Raster::Block4 & raster_v_subblock,
            unsigned int column_sb, unsigned int row_sb ) {
              UVBlock & frame_v_subblock = frame_macroblock.V().at( column_sb, row_sb );

              frame_v_subblock.mutable_coefficients().subtract_dct( raster_v_subblock,
                                                                    TwoDSubRange< uint8_t, 4, 4 >( prediction, 4 * column_sb, 4 * row_sb ) );
              frame_v_subblock.mutable_coefficients() = UVBlock::quantize( quantizer, frame_v_subblock.coefficients() );
              frame_v_subblock.calculate_has_nonzero();
            } );
      } );

    frame.relink_y2_blocks();
    ivf_writer_.append_frame( frame.serialize( temp_state.probability_tables ) );
}
