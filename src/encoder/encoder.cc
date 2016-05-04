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
void Encoder::luma_mb_intra_predict( VP8Raster::Macroblock & original_mb,
                                     VP8Raster::Macroblock & constructed_mb,
                                     MacroblockType & frame_mb,
                                     Quantizer & quantizer )
{
  // Select the best prediction mode
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
          frame_sb.set_Y_without_Y2();
          frame_sb.calculate_has_nonzero();

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

  // Apply
  frame_mb.Y2().set_prediction_mode( min_prediction_mode );

  if ( min_prediction_mode != B_PRED ) { // if B_PRED is selected, it is already taken care of.
    SafeArray< int16_t, 16 > walsh_input;

    frame_mb.Y().forall_ij( [&] ( YBlock & frame_sb, unsigned int sb_column, unsigned int sb_row ) {
      auto & original_sb = original_mb.Y_sub.at( sb_column, sb_row );
      frame_sb.set_prediction_mode( KeyFrameMacroblock::implied_subblock_mode( min_prediction_mode ) );

      frame_sb.mutable_coefficients().subtract_dct( original_sb,
        TwoDSubRange< uint8_t, 4, 4 >( min_prediction, 4 * sb_column, 4 * sb_row ) );

      walsh_input.at( sb_column + 4 * sb_row ) = frame_sb.coefficients().at( 0 );
      frame_sb.set_dc_coefficient( 0 );
      frame_sb.mutable_coefficients() = YBlock::quantize( quantizer, frame_sb.coefficients() );
      frame_sb.set_Y_after_Y2();
      frame_sb.calculate_has_nonzero();
    } );

    frame_mb.Y2().set_coded( true );
    frame_mb.Y2().mutable_coefficients().wht( walsh_input );
    frame_mb.Y2().mutable_coefficients() = Y2Block::quantize( quantizer, frame_mb.Y2().coefficients() );
    frame_mb.Y2().calculate_has_nonzero();
  }
}

template <class MacroblockType>
void Encoder::chroma_mb_intra_predict( VP8Raster::Macroblock & original_mb,
                                       VP8Raster::Macroblock & constructed_mb,
                                       MacroblockType & frame_mb,
                                       Quantizer & quantizer )
{
  // Select the best prediction mode
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

  // Apply
  frame_mb.U().at( 0, 0 ).set_prediction_mode( min_prediction_mode );

  frame_mb.U().forall_ij( [&] ( UVBlock & frame_sb,
    unsigned int sb_column, unsigned int sb_row ) {
      auto & original_sb = original_mb.U_sub.at( sb_column, sb_row );
      frame_sb.mutable_coefficients().subtract_dct( original_sb,
        TwoDSubRange< uint8_t, 4, 4 >( u_min_prediction, 4 * sb_column, 4 * sb_row ) );
      frame_sb.mutable_coefficients() = UVBlock::quantize( quantizer, frame_sb.coefficients() );
      frame_sb.calculate_has_nonzero();
    } );

  frame_mb.V().forall_ij( [&] ( UVBlock & frame_sb,
    unsigned int sb_column, unsigned int sb_row ) {
      auto & original_sb = original_mb.V_sub.at( sb_column, sb_row );
      frame_sb.mutable_coefficients().subtract_dct( original_sb,
        TwoDSubRange< uint8_t, 4, 4 >( v_min_prediction, 4 * sb_column, 4 * sb_row ) );
      frame_sb.mutable_coefficients() = UVBlock::quantize( quantizer, frame_sb.coefficients() );
      frame_sb.calculate_has_nonzero();
    } );
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

pair< KeyFrame, double > Encoder::get_encoded_keyframe( const VP8Raster & raster, const Optional<QuantIndices> quant_indices )
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

      // Process Y and Y2
      luma_mb_intra_predict( raster_macroblock, constructed_macroblock, frame_macroblock, quantizer );
      chroma_mb_intra_predict( raster_macroblock, constructed_macroblock, frame_macroblock, quantizer );

      frame.relink_y2_blocks();
      frame_macroblock.calculate_has_nonzero();
      frame_macroblock.reconstruct_intra( quantizer, constructed_macroblock );
    } );

    frame.relink_y2_blocks();

    return make_pair( move( frame ), constructed_raster.quality( raster ) );
}
