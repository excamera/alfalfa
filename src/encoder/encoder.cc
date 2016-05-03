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

template <class MacroblockType>
pair< mbmode, TwoD< uint8_t > > Encoder::luma_mb_intra_predict( VP8Raster::Macroblock & /* original_mb */,
                                                                VP8Raster::Macroblock & /* constructed_mb */,
                                                                MacroblockType & /* frame_mb */ )
{
  // TODO: dynamically choose the best prediction mode
  mbmode prediction_mode = B_PRED;
  TwoD< uint8_t > prediction( 16, 16 );
  prediction.fill( 0 );
  return make_pair( prediction_mode, move( prediction ) );
}

template <class MacroblockType>
pair< mbmode, TwoD< uint8_t > > Encoder::chroma_mb_intra_predict( VP8Raster::Macroblock & /* original_mb */,
                                                                  VP8Raster::Macroblock & constructed_mb,
                                                                  MacroblockType & /* frame_mb */,
                                                                  Component component )
{
  assert( component != Y_COMPONENT );

  // TODO: dynamically choose the best prediction mode
  mbmode prediction_mode = DC_PRED;
  TwoD< uint8_t > prediction( 8, 8 );

  if ( component == U_COMPONENT ) {
    constructed_mb.U.intra_predict( prediction_mode, prediction );
  }
  else {
    constructed_mb.V.intra_predict( prediction_mode, prediction );
  }

  return make_pair( prediction_mode, move( prediction ) );
}

template <class MacroblockType>
pair< bmode, TwoD< uint8_t > > Encoder::luma_sb_intra_predict( VP8Raster::Macroblock & /* original_mb */,
                                                                VP8Raster::Macroblock & /* constructed_mb */,
                                                                MacroblockType & frame_mb,
                                                                VP8Raster::Block4 & constructed_subblock )
{
  assert( frame_mb.Y2().prediction_mode() == B_PRED );

  bmode prediction_mode = B_DC_PRED;
  TwoD< uint8_t > prediction( 4, 4 );
  constructed_subblock.intra_predict( prediction_mode, prediction );

  return make_pair( prediction_mode, move( prediction ) );
}

void Encoder::encode_as_keyframe( const VP8Raster & raster, double )
{
  unsigned int width = raster.display_width();
  unsigned int height = raster.display_height();

  KeyFrame frame = Encoder::make_empty_frame( width, height );
  DecoderState temp_state { width, height };
  Quantizer quantizer( frame.header().quant_indices );
  MutableRasterHandle constructed_raster_handle { width, height };
  VP8Raster & constructed_raster = constructed_raster_handle.get();

  raster.macroblocks().forall_ij( [&] ( VP8Raster::Macroblock & raster_macroblock,
    unsigned int mb_column, unsigned int mb_row ) {
      auto & constructed_macroblock = constructed_raster.macroblock( mb_column, mb_row );
      auto & frame_macroblock = frame.mutable_macroblocks().at( mb_column, mb_row );

      // Process Y
      pair< mbmode, TwoD< uint8_t > > y_prediction = luma_mb_intra_predict( raster_macroblock,
                                                                            constructed_macroblock,
                                                                            frame_macroblock );
      frame_macroblock.Y2().set_prediction_mode( y_prediction.first );

      raster_macroblock.Y_sub.forall_ij( [&] ( VP8Raster::Block4 & raster_y_subblock,
        unsigned int sb_column, unsigned int sb_row ) {
          VP8Raster::Block4 & constructed_y_subblock = constructed_macroblock.Y_sub.at( sb_column, sb_row );
          YBlock & frame_y_subblock = frame_macroblock.Y().at( sb_column, sb_row );

          if ( y_prediction.first != B_PRED ) {
            frame_y_subblock.mutable_coefficients()
                            .subtract_dct( raster_y_subblock,
                                           TwoDSubRange< uint8_t, 4, 4 >( y_prediction.second, 4 * sb_column, 4 * sb_row ) );
          }
          else {
            pair< bmode, TwoD< uint8_t > > y_sb_prediction = luma_sb_intra_predict( raster_macroblock,
                                                                                    constructed_macroblock,
                                                                                    frame_macroblock,
                                                                                    constructed_y_subblock );
            frame_y_subblock.mutable_coefficients()
                            .subtract_dct( raster_y_subblock,
                                           TwoDSubRange< uint8_t, 4, 4 >( y_sb_prediction.second, 0, 0 ) );
            frame_y_subblock.set_prediction_mode( y_sb_prediction.first );
          }

          frame_y_subblock.mutable_coefficients() = YBlock::quantize( quantizer, frame_y_subblock.coefficients() );

          constructed_y_subblock.intra_predict( frame_y_subblock.prediction_mode() );
          frame_y_subblock.dequantize( quantizer ).idct_add( constructed_y_subblock );

          if ( y_prediction.first != B_PRED ) {
            frame_y_subblock.set_Y_after_Y2();
          }
          else {
            frame_y_subblock.set_Y_without_Y2();
          }

          frame_y_subblock.calculate_has_nonzero();
        } );

        // Process Y2
        // Not needed for now
        frame_macroblock.Y2().set_coded( false );

        // Process U
        pair< mbmode, TwoD< uint8_t > > u_prediction = chroma_mb_intra_predict( raster_macroblock,
                                                                                constructed_macroblock,
                                                                                frame_macroblock,
                                                                                U_COMPONENT );

        frame_macroblock.U().at( 0, 0 ).set_prediction_mode( u_prediction.first );

        raster_macroblock.U_sub.forall_ij( [&] ( VP8Raster::Block4 & raster_u_subblock,
          unsigned int sb_column, unsigned int sb_row ) {
            UVBlock & frame_u_subblock = frame_macroblock.U().at( sb_column, sb_row );
            frame_u_subblock.mutable_coefficients()
                            .subtract_dct( raster_u_subblock,
                                           TwoDSubRange< uint8_t, 4, 4 >( u_prediction.second, 4 * sb_column, 4 * sb_row ) );
            frame_u_subblock.mutable_coefficients() = UVBlock::quantize( quantizer, frame_u_subblock.coefficients() );
            frame_u_subblock.calculate_has_nonzero();
          } );

          // Process V
          pair< mbmode, TwoD< uint8_t > > v_prediction = chroma_mb_intra_predict( raster_macroblock,
                                                                                  constructed_macroblock,
                                                                                  frame_macroblock,
                                                                                  V_COMPONENT );

          assert( v_prediction.first == u_prediction.first ); // same prediction mode for both U and V

          raster_macroblock.V_sub.forall_ij( [&] ( VP8Raster::Block4 & raster_v_subblock,
            unsigned int sb_column, unsigned int sb_row ) {
              UVBlock & frame_v_subblock = frame_macroblock.V().at( sb_column, sb_row );
              frame_v_subblock.mutable_coefficients()
                              .subtract_dct( raster_v_subblock,
                                             TwoDSubRange< uint8_t, 4, 4 >( v_prediction.second, 4 * sb_column, 4 * sb_row ) );
              frame_v_subblock.mutable_coefficients() = UVBlock::quantize( quantizer, frame_v_subblock.coefficients() );
              frame_v_subblock.calculate_has_nonzero();
            } );

          // update reconstructed raster
          frame.relink_y2_blocks();
          frame_macroblock.calculate_has_nonzero();
          frame_macroblock.reconstruct_intra( quantizer, constructed_macroblock );
      } );

    frame.relink_y2_blocks();
    ivf_writer_.append_frame( frame.serialize( temp_state.probability_tables ) );
}
