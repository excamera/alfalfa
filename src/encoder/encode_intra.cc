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

#include <limits>
#include <typeinfo>

#include "encoder.hh"

using namespace std;

template<>
void Encoder::update_decoder_state( const KeyFrame & frame )
{
  // this is a keyframe! reset the decoder state
  decoder_state_ = DecoderState( frame.header(), width(), height() );
  references_ = References( width(), height() );

  if ( frame.header().refresh_entropy_probs ) {
    decoder_state_.probability_tables.coeff_prob_update( frame.header() );
  }
}

void Encoder::luma_sb_apply_intra_prediction( const VP8Raster::Block4 & original_sb,
                                              VP8Raster::Block4 & reconstructed_sb,
                                              YBlock & frame_sb,
                                              const Quantizer & quantizer,
                                              bmode sb_prediction_mode,
                                              const EncoderPass encoder_pass ) const
{
  frame_sb.mutable_coefficients().subtract_dct( original_sb,
    reconstructed_sb.contents() );

  if ( encoder_pass == FIRST_PASS ) {
    frame_sb.mutable_coefficients() = YBlock::quantize( quantizer, frame_sb.coefficients() );
  }
  else {
    trellis_quantize( frame_sb, quantizer );
  }

  frame_sb.set_prediction_mode( sb_prediction_mode );
  frame_sb.set_Y_without_Y2();
  frame_sb.calculate_has_nonzero();

  reconstructed_sb.intra_predict( sb_prediction_mode );
  frame_sb.dequantize( quantizer ).idct_add( reconstructed_sb );
}

/*
 * This function selects the best intra-prediction mode based on rd-cost.
 * In the case of B_PRED mode, the image is already reconstructed in
 * `reconstructed_mb` and the coefficients are set in `frame_mb`.
 * For any other prediction mode, `reconstructed_mb` will contain the
 * prediction values and no changes will be made in `frame_mb`. In this case,
 * there is another function that take cares of the reconstruction and filling
 * `frame_mb`.
 */
template<class MacroblockType>
Encoder::MBPredictionData Encoder::luma_mb_best_prediction_mode( const VP8Raster::Macroblock & original_mb,
                                                                 VP8Raster::Macroblock & reconstructed_mb,
                                                                 VP8Raster::Macroblock & temp_mb,
                                                                 MacroblockType & frame_mb,
                                                                 const Quantizer & quantizer,
                                                                 const EncoderPass encoder_pass,
                                                                 const bool interframe ) const
{
  MBPredictionData best_pred;

  TwoDSubRange<uint8_t, 16, 16> & prediction = temp_mb.Y.mutable_contents();
  auto predictors = reconstructed_mb.Y.predictors();

  unsigned int total_modes = B_PRED;

  if ( encode_quality_ == REALTIME_QUALITY and typeid( frame_mb ) == typeid( InterFrameMacroblock ) ) {
    // When running the real time mode, we don't consider B_PRED for inter-frames
    // macroblocks.
    total_modes = B_PRED - 1;
  }

  /* Because of the way that reconstructed_mb is used as a buffer to store the
   * best prediction result, it is necessary to first examine the B_PRED and
   * then the other prediction modes. */
  for ( unsigned int prediction_mode = total_modes; prediction_mode < num_y_modes; prediction_mode-- ) {
    MBPredictionData pred;
    pred.prediction_mode = ( mbmode )prediction_mode;

    if ( prediction_mode == B_PRED ) {
      pred.cost = 0;
      pred.rate = costs_.mbmode_costs.at( interframe ? 1 : 0 ).at( B_PRED );
      pred.distortion = 0;

      reconstructed_mb.Y_sub_forall_ij(
        [&] ( VP8Raster::Block4 & reconstructed_sb, unsigned int sb_column, unsigned int sb_row )
        {
          auto & original_sb = original_mb.Y_sub_at( sb_column, sb_row );
          auto & temp_sb = temp_mb.Y_sub_at( sb_column, sb_row );
          auto & frame_sb = frame_mb.Y().at( sb_column, sb_row );

          const auto above_mode = frame_sb.context().above.initialized()
            ? frame_sb.context().above.get()->prediction_mode() : B_DC_PRED;
          const auto left_mode = frame_sb.context().left.initialized()
            ? frame_sb.context().left.get()->prediction_mode() : B_DC_PRED;

          bmode sb_prediction_mode = luma_sb_intra_predict( original_sb,
            reconstructed_sb, temp_sb, costs_.bmode_costs.at( above_mode ).at( left_mode ) );

          pred.rate += costs_.bmode_costs.at( above_mode ).at( left_mode ).at( sb_prediction_mode );
          pred.distortion += sse( original_sb, reconstructed_sb.contents() );

          luma_sb_apply_intra_prediction( original_sb, reconstructed_sb, frame_sb,
                                          quantizer, sb_prediction_mode, encoder_pass );
        }
      );

      pred.cost = rdcost( pred.rate, pred.distortion,
                          RATE_MULTIPLIER, DISTORTION_MULTIPLIER );
    }
    else {
      reconstructed_mb.Y.intra_predict( ( mbmode )prediction_mode, predictors, prediction );

      /* Here we compute variance, instead of SSE, because in this case
       * the average will be taken out from Y2 block into the Y2 block. */
      pred.distortion = variance( original_mb.Y, prediction );

      pred.rate = costs_.mbmode_costs.at( interframe ? 1 : 0 ).at( prediction_mode );
      pred.cost = rdcost( pred.rate, pred.distortion, RATE_MULTIPLIER,
                          DISTORTION_MULTIPLIER );
    }

    if ( pred.cost < best_pred.cost ) {
      reconstructed_mb.Y.mutable_contents().copy_from( prediction );
      best_pred = pred;
    }
  }

  return best_pred;
}

/*
 * `reconstructed_mb` must contain the prediction values, or in the case of
 * B_PRED mode, it should contain the actual reconstruted frame, alongside with
 * corresponding coefficients in `frame_mb`.
 */
template<class MacroblockType>
void Encoder::luma_mb_apply_intra_prediction( const VP8Raster::Macroblock & original_mb,
                                              VP8Raster::Macroblock & reconstructed_mb,
                                              __attribute__((unused)) VP8Raster::Macroblock & temp_mb,
                                              MacroblockType & frame_mb,
                                              const Quantizer & quantizer,
                                              const mbmode min_prediction_mode,
                                              const EncoderPass encoder_pass ) const
{
  frame_mb.Y2().set_prediction_mode( min_prediction_mode );

  if ( min_prediction_mode == B_PRED ) {
    frame_mb.Y2().set_coded( false );
    return;
  }

  SafeArray<int16_t, 16> walsh_input;

  frame_mb.Y().forall_ij(
    [&] ( YBlock & frame_sb, unsigned int sb_column, unsigned int sb_row )
    {
      auto & original_sb = original_mb.Y_sub_at( sb_column, sb_row );
      frame_sb.set_prediction_mode( KeyFrameMacroblock::implied_subblock_mode( min_prediction_mode ) );

      frame_sb.mutable_coefficients().subtract_dct( original_sb,
        reconstructed_mb.Y_sub_at( sb_column, sb_row ).contents() );

      walsh_input.at( sb_column + 4 * sb_row ) = frame_sb.coefficients().at( 0 );
      frame_sb.set_dc_coefficient( 0 );
      frame_sb.set_Y_after_Y2();

      if ( encoder_pass == FIRST_PASS ) {
        frame_sb.mutable_coefficients() = YBlock::quantize( quantizer, frame_sb.coefficients() );
      }
      else {
        trellis_quantize( frame_sb, quantizer );
      }

      frame_sb.calculate_has_nonzero();
    }
  );

  frame_mb.Y2().set_coded( true );
  frame_mb.Y2().mutable_coefficients().wht( walsh_input );

  if ( encoder_pass == FIRST_PASS ) {
    frame_mb.Y2().mutable_coefficients() = Y2Block::quantize( quantizer, frame_mb.Y2().coefficients() );
  }
  else {
    check_reset_y2( frame_mb.Y2(), quantizer );
    trellis_quantize( frame_mb.Y2(), quantizer );
  }

  frame_mb.Y2().calculate_has_nonzero();
}

template <class MacroblockType>
void Encoder::luma_mb_intra_predict( const VP8Raster::Macroblock & original_mb,
                                     VP8Raster::Macroblock & reconstructed_mb,
                                     VP8Raster::Macroblock & temp_mb,
                                     MacroblockType & frame_mb,
                                     const Quantizer & quantizer,
                                     const EncoderPass encoder_pass ) const
{
  // Select the best prediction mode
  MBPredictionData best_pred = luma_mb_best_prediction_mode( original_mb,
                                                             reconstructed_mb,
                                                             temp_mb,
                                                             frame_mb,
                                                             quantizer,
                                                             encoder_pass );
  // Apply
  luma_mb_apply_intra_prediction( original_mb, reconstructed_mb, temp_mb,
                                  frame_mb, quantizer,
                                  best_pred.prediction_mode, encoder_pass );
}

/*
 * Please take a look at comments for `luma_mb_best_prediction_mode`.
 */
Encoder::MBPredictionData Encoder::chroma_mb_best_prediction_mode( const VP8Raster::Macroblock & original_mb,
                                                                   VP8Raster::Macroblock & reconstructed_mb,
                                                                   VP8Raster::Macroblock & temp_mb,
                                                                   const bool interframe ) const
{
  MBPredictionData best_pred;

  TwoDSubRange<uint8_t, 8, 8> & u_prediction = temp_mb.U.mutable_contents();
  TwoDSubRange<uint8_t, 8, 8> & v_prediction = temp_mb.V.mutable_contents();

  auto u_predictors = reconstructed_mb.U.predictors();
  auto v_predictors = reconstructed_mb.V.predictors();

  for ( unsigned int prediction_mode = 0; prediction_mode < num_uv_modes; prediction_mode++ ) {
    MBPredictionData pred;
    pred.prediction_mode = ( mbmode )prediction_mode;

    reconstructed_mb.U.intra_predict( ( mbmode )prediction_mode, u_predictors, u_prediction );
    reconstructed_mb.V.intra_predict( ( mbmode )prediction_mode, v_predictors, v_prediction );

    pred.distortion = sse( original_mb.U, u_prediction )
                    + sse( original_mb.V, v_prediction );

    pred.rate = costs_.intra_uv_mode_costs.at( interframe ).at( prediction_mode );
    pred.cost = rdcost( pred.rate, pred.distortion, RATE_MULTIPLIER,
                        DISTORTION_MULTIPLIER );

    if ( pred.distortion < best_pred.distortion ) {
      reconstructed_mb.U.mutable_contents().copy_from( u_prediction );
      reconstructed_mb.V.mutable_contents().copy_from( v_prediction );

      best_pred = pred;
    }
  }

  return best_pred;
}

template<class MacroblockType>
void Encoder::chroma_mb_apply_intra_prediction( const VP8Raster::Macroblock & original_mb,
                                                VP8Raster::Macroblock & reconstructed_mb,
                                                __attribute__((unused)) VP8Raster::Macroblock & temp_mb,
                                                MacroblockType & frame_mb,
                                                const Quantizer & quantizer,
                                                const mbmode min_prediction_mode,
                                                const EncoderPass encoder_pass ) const
{
  frame_mb.U().at( 0, 0 ).set_prediction_mode( min_prediction_mode );

  frame_mb.U().forall_ij(
    [&] ( UVBlock & frame_sb, unsigned int sb_column, unsigned int sb_row )
    {
      auto & original_sb = original_mb.U_sub_at( sb_column, sb_row );

      frame_sb.mutable_coefficients().subtract_dct( original_sb,
        reconstructed_mb.U_sub_at( sb_column, sb_row ).contents() );

      if ( encoder_pass == FIRST_PASS ) {
        frame_sb.mutable_coefficients() = UVBlock::quantize( quantizer, frame_sb.coefficients() );
      }
      else {
        trellis_quantize( frame_sb, quantizer );
      }

      frame_sb.calculate_has_nonzero();
    }
  );

  frame_mb.V().forall_ij(
    [&] ( UVBlock & frame_sb, unsigned int sb_column, unsigned int sb_row )
    {
      auto & original_sb = original_mb.V_sub_at( sb_column, sb_row );

      frame_sb.mutable_coefficients().subtract_dct( original_sb,
        reconstructed_mb.V_sub_at( sb_column, sb_row ).contents() );

      if ( encoder_pass == FIRST_PASS ) {
        frame_sb.mutable_coefficients() = UVBlock::quantize( quantizer, frame_sb.coefficients() );
      }
      else {
        trellis_quantize( frame_sb, quantizer );
      }

      frame_sb.calculate_has_nonzero();
    }
  );
}

template <class MacroblockType>
void Encoder::chroma_mb_intra_predict( const VP8Raster::Macroblock & original_mb,
                                       VP8Raster::Macroblock & reconstructed_mb,
                                       VP8Raster::Macroblock & temp_mb,
                                       MacroblockType & frame_mb,
                                       const Quantizer & quantizer,
                                       const EncoderPass encoder_pass,
                                       const bool interframe ) const
{
  // Select the best prediction mode
  MBPredictionData best_pred = chroma_mb_best_prediction_mode( original_mb,
                                                               reconstructed_mb,
                                                               temp_mb,
                                                               interframe );

  // Apply
  chroma_mb_apply_intra_prediction( original_mb, reconstructed_mb, temp_mb,
                                    frame_mb, quantizer,
                                    best_pred.prediction_mode, encoder_pass );
}

/* This function outputs the prediction values to 'reconstructed_sb'
 * and returns the prediction mode.
 */
bmode Encoder::luma_sb_intra_predict( const VP8Raster::Block4 & original_sb,
                                      VP8Raster::Block4 & reconstructed_sb,
                                      VP8Raster::Block4 & temp_sb,
                                      const SafeArray<uint16_t, num_intra_b_modes> & mode_costs ) const
{
  uint32_t min_error = numeric_limits<uint32_t>::max();
  bmode min_prediction_mode = B_DC_PRED;
  TwoDSubRange<uint8_t, 4, 4> & prediction = temp_sb.mutable_contents();

  auto predictors = reconstructed_sb.predictors();

  for ( unsigned int prediction_mode = 0; prediction_mode < num_intra_b_modes; prediction_mode++ ) {
    reconstructed_sb.intra_predict( ( bmode )prediction_mode, predictors, prediction );

    uint32_t distortion = sse( original_sb, prediction );
    uint32_t error_val = rdcost( mode_costs.at( prediction_mode ), distortion,
                                 RATE_MULTIPLIER, DISTORTION_MULTIPLIER );

    if ( error_val < min_error ) {
      reconstructed_sb.mutable_contents().copy_from( prediction );
      min_prediction_mode = ( bmode )prediction_mode;
      min_error = error_val;
    }
  }

  return min_prediction_mode;
}

template<>
pair<KeyFrame &, double> Encoder::encode_raster<KeyFrame>( const VP8Raster & raster,
                                                           const QuantIndices & quant_indices,
                                                           const bool update_state,
                                                           const bool compute_ssim )
{
  DecoderState decoder_state_copy = decoder_state_;
  decoder_state_ = DecoderState( width(), height() );

  KeyFrame & frame = key_frame_;

  frame.mutable_header().quant_indices = quant_indices;
  frame.mutable_header().refresh_entropy_probs = true;

  Quantizer quantizer( frame.header().quant_indices );
  MutableRasterHandle reconstructed_raster_handle { width(), height() };

  update_rd_multipliers( quantizer );

  TokenBranchCounts token_branch_counts;

  for ( size_t pass = FIRST_PASS;
        pass <= ( two_pass_encoder_ ? SECOND_PASS : FIRST_PASS );
        pass++ ) {

    if ( pass == SECOND_PASS ) {
      costs_.fill_token_costs( decoder_state_.probability_tables );
      token_branch_counts = TokenBranchCounts();
    }

    raster.macroblocks_forall_ij(
      [&] ( VP8Raster::ConstMacroblock original_mb, unsigned int mb_column, unsigned int mb_row )
      {
        auto reconstructed_mb = reconstructed_raster_handle.get().macroblock( mb_column, mb_row );
        auto temp_mb = temp_raster().macroblock( mb_column, mb_row );
        auto & frame_mb = frame.mutable_macroblocks().at( mb_column, mb_row );

        // Process Y and Y2
        luma_mb_intra_predict( original_mb.macroblock(), reconstructed_mb, temp_mb,
                               frame_mb, quantizer, (EncoderPass)pass );
        chroma_mb_intra_predict( original_mb.macroblock(), reconstructed_mb, temp_mb,
                                 frame_mb, quantizer, (EncoderPass)pass );

        frame_mb.calculate_has_nonzero();
        frame_mb.reconstruct_intra( quantizer, reconstructed_mb );

        frame_mb.accumulate_token_branches( token_branch_counts );
      }
    );

    optimize_probability_tables( frame, token_branch_counts );
  }

  optimize_prob_skip( frame );

  frame.relink_y2_blocks();

  // optimize_prob_skip( frame );
  apply_best_loopfilter_settings( raster, reconstructed_raster_handle.get(), frame );

  RasterHandle immutable_raster( move( reconstructed_raster_handle ) );

  if ( not update_state ) {
    decoder_state_ = decoder_state_copy;
  }

  return { frame,
           compute_ssim ? immutable_raster.get().quality( raster ) : 0.0 };
}
