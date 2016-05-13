#include <limits>

#include "encoder.hh"
#include "frame_header.hh"

using namespace std;

static unsigned calc_prob( unsigned false_count, unsigned total )
{
  if ( false_count == 0 ) {
    return 0;
  } else {
    return max( 1u, min( 255u, 256 * false_count / total ) );
  }
}

QuantIndices::QuantIndices()
  : y_ac_qi(), y_dc(), y2_dc(), y2_ac(), uv_dc(), uv_ac()
{}

KeyFrame Encoder::make_empty_frame( const uint16_t width, const uint16_t height )
{
  BoolDecoder data { { nullptr, 0 } };
  KeyFrame frame { true, width, height, data };
  frame.parse_macroblock_headers( data, ProbabilityTables {} );
  return frame;
}

template <unsigned int size>
template <class PredictionMode>
void VP8Raster::Block<size>::intra_predict( const PredictionMode mb_mode, TwoD<uint8_t> & output )
{
  TwoDSubRange<uint8_t, size, size> subrange( output, 0, 0 );
  intra_predict( mb_mode, subrange );
}

/* Encoder */
Encoder::Encoder( const string & output_filename, const uint16_t width, const uint16_t height )
  : ivf_writer_( output_filename, "VP80", width, height, 1, 1 ), width_( width ), height_( height )
{}

template<unsigned int size>
uint32_t Encoder::variance( const VP8Raster::Block<size> & block,
                            const TwoD<uint8_t> & prediction,
                            const uint16_t dc_factor, const uint16_t ac_factor )
{
  return Encoder::variance( block, TwoDSubRange<uint8_t, size, size>( prediction, 0, 0 ),
                            dc_factor, ac_factor );
}

template<unsigned int size>
uint32_t Encoder::variance( const VP8Raster::Block<size> & block,
                            const TwoDSubRange<uint8_t, size, size> & prediction,
                            const uint16_t dc_factor, const uint16_t ac_factor )
{
  uint32_t sse = 0;
  int32_t sum = 0;

  for ( size_t i = 0; i < size; i++ ) {
    for ( size_t j = 0; j < size; j++ ) {
      int16_t diff = ( block.at( i, j ) - prediction.at( i, j ) ) / ( ( i + j == 0 ) ? dc_factor : ac_factor );
      sum += diff;
      sse += diff * diff;
    }
  }

  return sse - ( ( int64_t )sum * sum ) / ( size * size );
}

template <class MacroblockType>
void Encoder::luma_mb_intra_predict( const VP8Raster::Macroblock & original_mb,
                                     VP8Raster::Macroblock & reconstructed_mb,
                                     MacroblockType & frame_mb,
                                     const Quantizer & quantizer ) const
{
  // Select the best prediction mode
  uint32_t min_energy = numeric_limits<uint32_t>::max();
  mbmode min_prediction_mode = DC_PRED;
  TwoD<uint8_t> min_prediction( 16, 16 );

  for ( unsigned int prediction_mode = 0; prediction_mode < num_y_modes; prediction_mode++ ) {
    TwoD<uint8_t> prediction( 16, 16 );
    uint32_t variance_val = numeric_limits<uint32_t>::max();

    if ( prediction_mode == B_PRED ) {
      prediction.fill( 0 );
      variance_val = 0;

      reconstructed_mb.Y_sub.forall_ij(
        [&] ( VP8Raster::Block4 & reconstructed_sb, unsigned int sb_column, unsigned int sb_row )
        {
          auto & original_sb = original_mb.Y_sub.at( sb_column, sb_row );
          auto & frame_sb = frame_mb.Y().at( sb_column, sb_row );

          pair<bmode, TwoD<uint8_t>> sb_prediction = luma_sb_intra_predict( original_sb,
            reconstructed_sb, frame_sb, quantizer );

          variance_val += variance( original_sb, sb_prediction.second,
            quantizer.y_dc, quantizer.y_ac );

          frame_sb.mutable_coefficients().subtract_dct( original_sb,
            TwoDSubRange<uint8_t, 4, 4>( sb_prediction.second, 0, 0 ) );

          frame_sb.mutable_coefficients() = YBlock::quantize( quantizer, frame_sb.coefficients() );
          frame_sb.set_prediction_mode( sb_prediction.first );
          frame_sb.set_Y_without_Y2();
          frame_sb.calculate_has_nonzero();

          reconstructed_sb.intra_predict( sb_prediction.first );
          frame_sb.dequantize( quantizer ).idct_add( reconstructed_sb );
        } );
    }
    else {
      reconstructed_mb.Y.intra_predict( ( mbmode )prediction_mode, prediction );
      variance_val = variance( original_mb.Y, prediction, quantizer.y_dc, quantizer.y_ac );
    }

    if ( variance_val < min_energy ) {
      min_prediction = move( prediction );
      min_prediction_mode = ( mbmode )prediction_mode;
      min_energy = variance_val;
    }
  }

  // Apply
  frame_mb.Y2().set_prediction_mode( min_prediction_mode );

  if ( min_prediction_mode != B_PRED ) { // if B_PRED is selected, it is already taken care of.
    SafeArray<int16_t, 16> walsh_input;

    frame_mb.Y().forall_ij(
      [&] ( YBlock & frame_sb, unsigned int sb_column, unsigned int sb_row )
      {
        auto & original_sb = original_mb.Y_sub.at( sb_column, sb_row );
        frame_sb.set_prediction_mode( KeyFrameMacroblock::implied_subblock_mode( min_prediction_mode ) );

        frame_sb.mutable_coefficients().subtract_dct( original_sb,
          TwoDSubRange<uint8_t, 4, 4>( min_prediction, 4 * sb_column, 4 * sb_row ) );

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
  else {
    frame_mb.Y2().set_coded( false );
  }
}

template <class MacroblockType>
void Encoder::chroma_mb_intra_predict( const VP8Raster::Macroblock & original_mb,
                                       VP8Raster::Macroblock & reconstructed_mb,
                                       MacroblockType & frame_mb,
                                       const Quantizer & quantizer ) const
{
  // Select the best prediction mode
  uint32_t min_energy = numeric_limits<uint32_t>::max();
  mbmode min_prediction_mode = DC_PRED;
  TwoD<uint8_t> u_min_prediction( 8, 8 );
  TwoD<uint8_t> v_min_prediction( 8, 8 );

  for ( unsigned int prediction_mode = 0; prediction_mode < num_uv_modes; prediction_mode++ ) {
    TwoD<uint8_t> u_prediction( 8, 8 );
    TwoD<uint8_t> v_prediction( 8, 8 );

    reconstructed_mb.U.intra_predict( ( mbmode )prediction_mode, u_prediction );
    reconstructed_mb.V.intra_predict( ( mbmode )prediction_mode, v_prediction );

    uint32_t variance_val = variance( original_mb.U, u_prediction, quantizer.uv_dc, quantizer.uv_ac );
    variance_val += variance( original_mb.V, v_prediction, quantizer.uv_dc, quantizer.uv_ac );

    if ( variance_val < min_energy ) {
      u_min_prediction = move( u_prediction );
      v_min_prediction = move( v_prediction );
      min_prediction_mode = ( mbmode )prediction_mode;
      min_energy = variance_val;
    }
  }

  // Apply
  frame_mb.U().at( 0, 0 ).set_prediction_mode( min_prediction_mode );

  frame_mb.U().forall_ij(
    [&] ( UVBlock & frame_sb, unsigned int sb_column, unsigned int sb_row )
    {
      auto & original_sb = original_mb.U_sub.at( sb_column, sb_row );
      frame_sb.mutable_coefficients().subtract_dct( original_sb,
        TwoDSubRange<uint8_t, 4, 4>( u_min_prediction, 4 * sb_column, 4 * sb_row ) );
      frame_sb.mutable_coefficients() = UVBlock::quantize( quantizer, frame_sb.coefficients() );
      frame_sb.calculate_has_nonzero();
    } );

  frame_mb.V().forall_ij(
    [&] ( UVBlock & frame_sb, unsigned int sb_column, unsigned int sb_row )
    {
      auto & original_sb = original_mb.V_sub.at( sb_column, sb_row );
      frame_sb.mutable_coefficients().subtract_dct( original_sb,
        TwoDSubRange<uint8_t, 4, 4>( v_min_prediction, 4 * sb_column, 4 * sb_row ) );
      frame_sb.mutable_coefficients() = UVBlock::quantize( quantizer, frame_sb.coefficients() );
      frame_sb.calculate_has_nonzero();
    } );
}

pair<bmode, TwoD<uint8_t>> Encoder::luma_sb_intra_predict( const VP8Raster::Block4 & original_sb,
                                                               VP8Raster::Block4 & reconstructed_sb,
                                                               YBlock & /* frame_sb */,
                                                               const Quantizer & quantizer ) const
{
  uint32_t min_energy = numeric_limits<uint32_t>::max();
  bmode min_prediction_mode = B_DC_PRED;
  TwoD<uint8_t> min_prediction( 4, 4 );

  for ( unsigned int prediction_mode = 0; prediction_mode < num_intra_b_modes; prediction_mode++ ) {
    TwoD<uint8_t> prediction( 4, 4 );

    reconstructed_sb.intra_predict( ( bmode )prediction_mode, prediction );
    uint32_t variance_val = variance( original_sb, prediction, quantizer.y_dc, quantizer.y_ac );

    if ( variance_val < min_energy ) {
      min_prediction = move( prediction );
      min_prediction_mode = ( bmode )prediction_mode;
      min_energy = variance_val;
    }
  }

  return make_pair( min_prediction_mode, move( min_prediction ) );
}

template<class FrameType>
void Encoder::optimize_coefficients( FrameType & frame ) const
{
  TokenBranchCounts token_branch_counts;
  frame.macroblocks().forall(
    [&] ( KeyFrameMacroblock & frame_mb )
    {
      frame_mb.accumulate_token_branches( token_branch_counts );
    }
  );

  for ( unsigned int i = 0; i < BLOCK_TYPES; i++ ) {
    for ( unsigned int j = 0; j < COEF_BANDS; j++ ) {
      for ( unsigned int k = 0; k < PREV_COEF_CONTEXTS; k++ ) {
        for ( unsigned int l = 0; l < ENTROPY_NODES; l++ ) {
          const unsigned int false_count = token_branch_counts.at( i ).at( j ).at( k ).at( l ).first;
          const unsigned int true_count = token_branch_counts.at( i ).at( j ).at( k ).at( l ).second;

          const unsigned int prob = calc_prob( false_count, false_count + true_count );

          assert( prob <= 255 );

          if ( prob > 0 and prob != k_default_coeff_probs.at( i ).at( j ).at( k ).at( l ) ) {
            frame.mutable_header().token_prob_update.at( i ).at( j ).at( k ).at( l ) = TokenProbUpdate( true, prob );
          }
        }
      }
    }
  }
}

template<>
pair<KeyFrame, double> Encoder::encode_with_quantizer<KeyFrame>( const VP8Raster & raster, const QuantIndices & quant_indices,
                                                                 const DecoderState & decoder_state ) const
{
  const uint16_t width = raster.display_width();
  const uint16_t height = raster.display_height();

  KeyFrame frame = Encoder::make_empty_frame( width, height );
  frame.mutable_header().quant_indices = quant_indices;

  Quantizer quantizer( frame.header().quant_indices );
  MutableRasterHandle reconstructed_raster_handle { width, height };
  VP8Raster & reconstructed_raster = reconstructed_raster_handle.get();

  raster.macroblocks().forall_ij(
    [&] ( VP8Raster::Macroblock & original_mb, unsigned int mb_column, unsigned int mb_row )
    {
      auto & reconstructed_mb = reconstructed_raster.macroblock( mb_column, mb_row );
      auto & frame_mb = frame.mutable_macroblocks().at( mb_column, mb_row );

      // Process Y and Y2
      luma_mb_intra_predict( original_mb, reconstructed_mb, frame_mb, quantizer );
      chroma_mb_intra_predict( original_mb, reconstructed_mb, frame_mb, quantizer );

      frame.relink_y2_blocks();
      frame_mb.calculate_has_nonzero();
      frame_mb.reconstruct_intra( quantizer, reconstructed_mb );
    }
  );

  optimize_coefficients( frame );
  frame.loopfilter( decoder_state.segmentation, decoder_state.filter_adjustments, reconstructed_raster );
  return make_pair( move( frame ), reconstructed_raster.quality( raster ) );
}


double Encoder::encode_as_keyframe( const VP8Raster & raster, const double minimum_ssim )
{
  int y_ac_qi_min = 0;
  int y_ac_qi_max = 127;

  const uint16_t width = raster.display_width();
  const uint16_t height = raster.display_height();

  if ( width != width_ or height != height_ ) {
    throw runtime_error( "scaling is not supported." );
  }

  DecoderState temp_state { width, height };
  QuantIndices quant_indices;
  quant_indices.y_dc  = Signed<4>( 0 );
  quant_indices.uv_dc = Signed<4>( 15 );
  quant_indices.uv_ac = Signed<4>( 15 );

  pair<KeyFrame, double> encoded_frame = make_pair( move( make_empty_frame( width, height ) ), 0 );

  while ( y_ac_qi_min <= y_ac_qi_max ) {
    quant_indices.y_ac_qi = ( y_ac_qi_min + y_ac_qi_max ) / 2;
    encoded_frame = encode_with_quantizer<KeyFrame>( raster, quant_indices, temp_state );

    if ( abs( encoded_frame.second - minimum_ssim ) < 0.005
         or y_ac_qi_min == y_ac_qi_max) {
      break; // "I believe the search is over."
    }

    if ( encoded_frame.second < minimum_ssim ) {
      y_ac_qi_max = ( y_ac_qi_min + y_ac_qi_max ) / 2 - 1;
    }
    else {
      y_ac_qi_min = ( y_ac_qi_min + y_ac_qi_max ) / 2 + 1;
    }
  }

  ivf_writer_.append_frame( encoded_frame.first.serialize( temp_state.probability_tables ) );

  return encoded_frame.second;
}
