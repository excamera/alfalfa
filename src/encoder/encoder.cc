#include <limits>

#include "encoder.hh"
#include "frame_header.hh"

using namespace std;

QuantIndices::QuantIndices()
  : y_ac_qi(), y_dc(), y2_dc(), y2_ac(), uv_dc(), uv_ac()
{}

KeyFrame Encoder::make_empty_frame( unsigned int width, unsigned int height )
{
  BoolDecoder data { { nullptr, 0 } };
  KeyFrame frame { true, width, height, data };
  frame.parse_macroblock_headers( data, ProbabilityTables {} );
  return frame;
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

template<unsigned int size>
uint64_t Encoder::get_energy( const VP8Raster::Block< size > & block,
                              const TwoD< uint8_t > & prediction,
                              const uint16_t dc_factor, const uint16_t ac_factor )
{
  return get_energy( block, TwoDSubRange< uint8_t, size, size >( prediction, 0, 0 ),
                     dc_factor, ac_factor );
}

template<unsigned int size>
uint64_t Encoder::get_energy( const VP8Raster::Block< size > & block,
                              const TwoDSubRange< uint8_t, size, size > & prediction,
                              const uint16_t dc_factor, const uint16_t ac_factor )
{
  uint64_t energy = 0;

  for ( size_t i = 0; i < size; i++ ) {
    for ( size_t j = 0; j < size; j++ ) {
      energy += ( block.at( i, j ) - prediction.at( i, j ) ) *
                ( block.at( i, j ) - prediction.at( i, j ) ) /
                ( ( i + j == 0 ) ? dc_factor : ac_factor );
    }
  }

  return energy;
}

template <class MacroblockType>
pair< mbmode, TwoD< uint8_t > > Encoder::luma_mb_intra_predict( VP8Raster::Macroblock & original_mb,
                                                                VP8Raster::Macroblock & constructed_mb,
                                                                MacroblockType & frame_mb,
                                                                Quantizer & quantizer )
{
  uint64_t min_energy = numeric_limits< uint64_t >::max();
  mbmode min_prediction_mode = DC_PRED;
  TwoD< uint8_t > min_prediction( 16, 16 );

  for ( int prediction_mode = DC_PRED; prediction_mode <= B_PRED; prediction_mode++ ) {
    TwoD< uint8_t > prediction( 16, 16 );
    uint64_t energy = numeric_limits< uint64_t >::max();

    if ( prediction_mode == B_PRED ) {
      prediction.fill( 0 );
      energy = 0;

      constructed_mb.Y_sub.forall_ij( [&] ( VP8Raster::Block4 & constructed_sb,
        unsigned int sb_column, unsigned int sb_row ) {
          auto & original_sb = original_mb.Y_sub.at( sb_column, sb_row );
          auto & frame_sb = frame_mb.Y().at( sb_column, sb_row );

          pair< bmode, TwoD< uint8_t > > sb_prediction = luma_sb_intra_predict( original_sb,
            constructed_sb, frame_sb, quantizer );

          energy += get_energy( original_sb, sb_prediction.second,
            quantizer.y_dc, quantizer.y_ac );

          frame_sb.mutable_coefficients().subtract_dct( original_sb,
            TwoDSubRange< uint8_t, 4, 4 >( sb_prediction.second, 0, 0 ) );

          frame_sb.mutable_coefficients() = YBlock::quantize( quantizer, frame_sb.coefficients() );
          frame_sb.set_prediction_mode( sb_prediction.first );
          constructed_sb.intra_predict( sb_prediction.first );
          frame_sb.dequantize( quantizer ).idct_add( constructed_sb );
        } );
    }
    else {
      constructed_mb.Y.intra_predict( ( mbmode )prediction_mode, prediction );
      energy = get_energy( original_mb.Y, prediction, quantizer.y_dc, quantizer.y_ac );
    }

    if ( energy < min_energy ) {
      min_prediction = move( prediction );
      min_prediction_mode = ( mbmode )prediction_mode;
      min_energy = energy;
    }
  }

  return make_pair( min_prediction_mode, move( min_prediction ) );
}

template <class MacroblockType>
tuple< mbmode, TwoD< uint8_t >, TwoD< uint8_t > > Encoder::chroma_mb_intra_predict( VP8Raster::Macroblock & original_mb,
                                                                                    VP8Raster::Macroblock & constructed_mb,
                                                                                    MacroblockType & /* frame_mb */,
                                                                                    Quantizer & quantizer )
{
  uint64_t min_energy = numeric_limits< uint64_t >::max();
  mbmode min_prediction_mode = DC_PRED;
  TwoD< uint8_t > u_min_prediction( 8, 8 );
  TwoD< uint8_t > v_min_prediction( 8, 8 );

  for ( int prediction_mode = DC_PRED; prediction_mode <= TM_PRED; prediction_mode++ ) {
    TwoD< uint8_t > u_prediction( 8, 8 );
    TwoD< uint8_t > v_prediction( 8, 8 );

    constructed_mb.U.intra_predict( ( mbmode )prediction_mode, u_prediction );
    constructed_mb.V.intra_predict( ( mbmode )prediction_mode, v_prediction );

    uint64_t energy = get_energy( original_mb.U, u_prediction, quantizer.uv_dc, quantizer.uv_ac );
    energy += get_energy( original_mb.V, v_prediction, quantizer.uv_dc, quantizer.uv_ac );

    if ( energy < min_energy ) {
      u_min_prediction = move( u_prediction );
      v_min_prediction = move( v_prediction );
      min_prediction_mode = ( mbmode )prediction_mode;
      min_energy = energy;
    }
  }

  return make_tuple( min_prediction_mode, move( u_min_prediction ), move( v_min_prediction ) );
}

pair< bmode, TwoD< uint8_t > > Encoder::luma_sb_intra_predict( VP8Raster::Block4 & original_sb,
                                                               VP8Raster::Block4 & constructed_sb,
                                                               YBlock & /* frame_sb */,
                                                               Quantizer & quantizer )
{
  uint64_t min_energy = numeric_limits< uint64_t >::max();
  bmode min_prediction_mode = B_DC_PRED;
  TwoD< uint8_t > min_prediction( 4, 4 );

  for ( size_t prediction_mode = B_DC_PRED; prediction_mode <= B_HU_PRED; prediction_mode++ ) {
    TwoD< uint8_t > prediction( 4, 4 );

    constructed_sb.intra_predict( ( bmode )prediction_mode, prediction );
    uint64_t energy = get_energy( original_sb, prediction, quantizer.y_dc, quantizer.y_ac );

    if ( energy < min_energy ) {
      min_prediction = move( prediction );
      min_prediction_mode = ( bmode )prediction_mode;
      min_energy = energy;
    }
  }

  return make_pair( min_prediction_mode, move( min_prediction ) );
}


double Encoder::encode_as_keyframe( const VP8Raster & raster, double minimum_ssim )
{
  uint8_t y_ac_qi_min = 0;
  uint8_t y_ac_qi_max = 127;

  unsigned int width = raster.display_width();
  unsigned int height = raster.display_height();

  DecoderState temp_state { width, height };
  QuantIndices quant_indices;

  while ( y_ac_qi_min < y_ac_qi_max ) {
    quant_indices.y_ac_qi = ( y_ac_qi_min + y_ac_qi_max ) / 2;
    pair< KeyFrame, double > frame_data = get_encoded_keyframe( raster, make_optional( true, quant_indices ) );

    if ( abs( frame_data.second - minimum_ssim ) < 0.005 ) {
      ivf_writer_.append_frame( frame_data.first.serialize( temp_state.probability_tables ) );
      return frame_data.second;
    }

    if ( frame_data.second < minimum_ssim ) {
      y_ac_qi_max = ( y_ac_qi_min + y_ac_qi_max ) / 2 - 1;
    }
    else {
      y_ac_qi_min = ( y_ac_qi_min + y_ac_qi_max ) / 2 + 1;
    }
  }

  pair< KeyFrame, double > frame_data = get_encoded_keyframe( raster, make_optional( true, quant_indices ) );
  ivf_writer_.append_frame( frame_data.first.serialize( temp_state.probability_tables ) );
  return frame_data.second;
}

std::pair< KeyFrame, double > Encoder::get_encoded_keyframe( const VP8Raster & raster, const Optional<QuantIndices> quant_indices )
{
  unsigned int width = raster.display_width();
  unsigned int height = raster.display_height();

  KeyFrame frame = Encoder::make_empty_frame( width, height );

  if ( quant_indices.initialized() ) {
    frame.mutable_header().quant_indices = quant_indices.get();
  }

  Quantizer quantizer( frame.header().quant_indices );
  MutableRasterHandle constructed_raster_handle { width, height };
  VP8Raster & constructed_raster = constructed_raster_handle.get();

  raster.macroblocks().forall_ij( [&] ( VP8Raster::Macroblock & raster_macroblock,
    unsigned int mb_column, unsigned int mb_row ) {
      auto & constructed_macroblock = constructed_raster.macroblock( mb_column, mb_row );
      auto & frame_macroblock = frame.mutable_macroblocks().at( mb_column, mb_row );

      // Process Y
      pair< mbmode, TwoD< uint8_t > > y_prediction = luma_mb_intra_predict( raster_macroblock,
        constructed_macroblock, frame_macroblock, quantizer );

      SafeArray< int16_t, 16 > walsh_input;

      frame_macroblock.Y2().set_prediction_mode( y_prediction.first );

      raster_macroblock.Y_sub.forall_ij( [&] ( VP8Raster::Block4 & raster_y_subblock,
        unsigned int sb_column, unsigned int sb_row ) {
          VP8Raster::Block4 & constructed_y_subblock = constructed_macroblock.Y_sub.at( sb_column, sb_row );
          YBlock & frame_y_subblock = frame_macroblock.Y().at( sb_column, sb_row );

          if ( y_prediction.first != B_PRED ) {
            frame_y_subblock.mutable_coefficients().subtract_dct( raster_y_subblock,
              TwoDSubRange< uint8_t, 4, 4 >( y_prediction.second, 4 * sb_column, 4 * sb_row ) );

            walsh_input.at( sb_column + 4 * sb_row ) = frame_y_subblock.coefficients().at( 0 );
            frame_y_subblock.set_dc_coefficient( 0 );
            frame_y_subblock.set_prediction_mode( KeyFrameMacroblock::implied_subblock_mode( y_prediction.first ) );
          }
          else {
            pair< bmode, TwoD< uint8_t > > y_sb_prediction = luma_sb_intra_predict( raster_y_subblock,
              constructed_y_subblock, frame_y_subblock, quantizer );

            frame_y_subblock.mutable_coefficients().subtract_dct( raster_y_subblock,
              TwoDSubRange< uint8_t, 4, 4 >( y_sb_prediction.second, 0, 0 ) );

            frame_y_subblock.set_prediction_mode( y_sb_prediction.first );
          }

          frame_y_subblock.mutable_coefficients() = YBlock::quantize( quantizer, frame_y_subblock.coefficients() );

          if ( y_prediction.first == B_PRED ) {
            constructed_y_subblock.intra_predict( frame_y_subblock.prediction_mode() );
            frame_y_subblock.dequantize( quantizer ).idct_add( constructed_y_subblock );
            frame_y_subblock.set_Y_without_Y2();
          }
          else {
            frame_y_subblock.set_Y_after_Y2();
          }

          frame_y_subblock.calculate_has_nonzero();
        } );

        // Process Y2
        frame_macroblock.Y2().set_coded( y_prediction.first != B_PRED );

        if ( y_prediction.first != B_PRED ) {
          frame_macroblock.Y2().mutable_coefficients().wht( walsh_input );
          frame_macroblock.Y2().mutable_coefficients() = Y2Block::quantize( quantizer, frame_macroblock.Y2().coefficients() );
          frame_macroblock.Y2().calculate_has_nonzero();
        }

        // Process U
        tuple< mbmode, TwoD< uint8_t >, TwoD< uint8_t > > uv_prediction = chroma_mb_intra_predict( raster_macroblock,
          constructed_macroblock, frame_macroblock, quantizer );

        frame_macroblock.U().at( 0, 0 ).set_prediction_mode( get<0>( uv_prediction ) );

        raster_macroblock.U_sub.forall_ij( [&] ( VP8Raster::Block4 & raster_u_subblock,
          unsigned int sb_column, unsigned int sb_row ) {
            UVBlock & frame_u_subblock = frame_macroblock.U().at( sb_column, sb_row );
            frame_u_subblock.mutable_coefficients().subtract_dct( raster_u_subblock,
              TwoDSubRange< uint8_t, 4, 4 >( get<1>( uv_prediction ), 4 * sb_column, 4 * sb_row ) );
            frame_u_subblock.mutable_coefficients() = UVBlock::quantize( quantizer, frame_u_subblock.coefficients() );
            frame_u_subblock.calculate_has_nonzero();
          } );

          // Process V
          raster_macroblock.V_sub.forall_ij( [&] ( VP8Raster::Block4 & raster_v_subblock,
            unsigned int sb_column, unsigned int sb_row ) {
              UVBlock & frame_v_subblock = frame_macroblock.V().at( sb_column, sb_row );
              frame_v_subblock.mutable_coefficients().subtract_dct( raster_v_subblock,
                TwoDSubRange< uint8_t, 4, 4 >( get<2>( uv_prediction ), 4 * sb_column, 4 * sb_row ) );
              frame_v_subblock.mutable_coefficients() = UVBlock::quantize( quantizer, frame_v_subblock.coefficients() );
              frame_v_subblock.calculate_has_nonzero();
            } );

          // update reconstructed raster
          frame.relink_y2_blocks();
          frame_macroblock.calculate_has_nonzero();
          frame_macroblock.reconstruct_intra( quantizer, constructed_macroblock );
      } );

    frame.relink_y2_blocks();

    return make_pair( move( frame ), constructed_raster.quality( raster ) );
}
