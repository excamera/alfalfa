/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <limits>

#include "encoder.hh"
#include "scorer.hh"

using namespace std;

void Encoder::luma_mb_inter_predict( const VP8Raster::Macroblock & original_mb,
                                     VP8Raster::Macroblock & reconstructed_mb,
                                     VP8Raster::Macroblock & temp_mb,
                                     InterFrameMacroblock & frame_mb,
                                     const Quantizer & quantizer,
                                     const EncoderPass encoder_pass )
{
  // let's find the best intra-prediction for this macroblock first...
  MBPredictionData best_pred = luma_mb_best_prediction_mode( original_mb,
                                                             reconstructed_mb,
                                                             temp_mb,
                                                             frame_mb,
                                                             quantizer,
                                                             encoder_pass,
                                                             true );

  const VP8Raster & reference = references_.last.get();
  const auto & reference_mb = reference.macroblock( frame_mb.context().column,
                                                    frame_mb.context().row );

  TwoDSubRange<uint8_t, 16, 16> & prediction = temp_mb.Y.mutable_contents();

  const Scorer census = frame_mb.motion_vector_census();
  const auto counts = census.mode_contexts();
  const ProbabilityArray< num_mv_refs > mv_ref_probs = {{ mv_counts_to_probs.at( counts.at( 0 ) ).at( 0 ),
                                                          mv_counts_to_probs.at( counts.at( 1 ) ).at( 1 ),
                                                          mv_counts_to_probs.at( counts.at( 2 ) ).at( 2 ),
                                                          mv_counts_to_probs.at( counts.at( 3 ) ).at( 3 ) }};

  costs_.fill_mv_ref_costs( mv_ref_probs );

  vector<mbmode> inter_modes = { ZEROMV, /* SPLIMV, NEARESTMV, NEARMV, NEWMV */ };

  for ( const mbmode prediction_mode : inter_modes ) {
    MBPredictionData pred;
    pred.prediction_mode = prediction_mode;

    reference_mb.Y.inter_predict( MotionVector(), reference.Y(), prediction );
    pred.distortion = variance( original_mb.Y, prediction );
    pred.rate = costs_.mbmode_costs.at( 1 ).at( prediction_mode );
    pred.cost = rdcost( pred.rate, pred.distortion, RATE_MULTIPLIER,
                        DISTORTION_MULTIPLIER );

    if ( pred.distortion < best_pred.distortion ) {
      best_pred = pred;
    }
  }

  if ( best_pred.prediction_mode <= B_PRED ) {
    // This block will be intra-predicted
    frame_mb.mutable_header().is_inter_mb = false;

    luma_mb_apply_intra_prediction( original_mb, reconstructed_mb, temp_mb,
                                    frame_mb, quantizer,
                                    best_pred.prediction_mode, encoder_pass );
  }
  else {
    // This block will be inter-predicted
    // Only ZEROMV is implemented for now.
    frame_mb.mutable_header().is_inter_mb = true;
    frame_mb.mutable_header().set_reference( LAST_FRAME );

    frame_mb.Y2().set_prediction_mode( ZEROMV );
    frame_mb.set_base_motion_vector( MotionVector() );

    frame_mb.Y().forall(
      [&] ( YBlock & frame_sb ) { frame_sb.set_motion_vector( frame_mb.base_motion_vector() ); }
    );

    reconstructed_mb.Y.mutable_contents().copy_from( prediction );

    SafeArray<int16_t, 16> walsh_input;

    // XXX refactor this and its keyframe counterpart to remove the redundancy.
    frame_mb.Y().forall_ij(
      [&] ( YBlock & frame_sb, unsigned int sb_column, unsigned int sb_row )
      {
        auto & original_sb = original_mb.Y_sub.at( sb_column, sb_row );

        frame_sb.mutable_coefficients().subtract_dct( original_sb,
          reconstructed_mb.Y_sub.at( sb_column, sb_row ).contents() );

        walsh_input.at( sb_column + 4 * sb_row ) = frame_sb.coefficients().at( 0 );
        frame_sb.set_dc_coefficient( 0 );
        frame_sb.set_Y_after_Y2();

        frame_sb.mutable_coefficients() = YBlock::quantize( quantizer, frame_sb.coefficients() );
        frame_sb.calculate_has_nonzero();
      }
    );

    frame_mb.Y2().set_coded( true );
    frame_mb.Y2().mutable_coefficients().wht( walsh_input );
    frame_mb.Y2().mutable_coefficients() = Y2Block::quantize( quantizer, frame_mb.Y2().coefficients() );
    frame_mb.Y2().calculate_has_nonzero();
  }
}

void Encoder::chroma_mb_inter_predict( const VP8Raster::Macroblock & original_mb,
                                       VP8Raster::Macroblock & reconstructed_mb,
                                       VP8Raster::Macroblock & temp_mb,
                                       InterFrameMacroblock & frame_mb,
                                       const Quantizer & quantizer,
                                       const EncoderPass ) const
{
  assert( frame_mb.inter_coded() );

  const VP8Raster & reference = references_.last.get();
  const auto & reference_mb = reference.macroblock( frame_mb.context().column,
                                                    frame_mb.context().row );

  TwoDSubRange<uint8_t, 8, 8> & u_prediction = temp_mb.U.mutable_contents();
  TwoDSubRange<uint8_t, 8, 8> & v_prediction = temp_mb.V.mutable_contents();

  frame_mb.U().forall_ij(
    [&]( UVBlock & block, const unsigned int column, const unsigned int row )
    {
      block.set_motion_vector( MotionVector::luma_to_chroma(
        frame_mb.Y().at( column * 2,     row * 2     ).motion_vector(),
        frame_mb.Y().at( column * 2 + 1, row * 2     ).motion_vector(),
        frame_mb.Y().at( column * 2,     row * 2 + 1 ).motion_vector(),
        frame_mb.Y().at( column * 2 + 1, row * 2 + 1 ).motion_vector() ) );
    }
  );

  reference_mb.U.inter_predict( frame_mb.U().at( 0, 0 ).motion_vector(),
                                reference.U(), u_prediction );
  reference_mb.V.inter_predict( frame_mb.U().at( 0, 0 ).motion_vector(),
                                reference.V(), v_prediction );

  reconstructed_mb.U.mutable_contents().copy_from( u_prediction );
  reconstructed_mb.V.mutable_contents().copy_from( v_prediction );

  frame_mb.U().forall_ij(
    [&] ( UVBlock & frame_sb, unsigned int sb_column, unsigned int sb_row )
    {
      auto & original_sb = original_mb.U_sub.at( sb_column, sb_row );

      frame_sb.mutable_coefficients().subtract_dct( original_sb,
        reconstructed_mb.U_sub.at( sb_column, sb_row ).contents() );

      frame_sb.mutable_coefficients() = UVBlock::quantize( quantizer, frame_sb.coefficients() );
      frame_sb.calculate_has_nonzero();
    }
  );

  frame_mb.V().forall_ij(
    [&] ( UVBlock & frame_sb, unsigned int sb_column, unsigned int sb_row )
    {
      auto & original_sb = original_mb.V_sub.at( sb_column, sb_row );

      frame_sb.mutable_coefficients().subtract_dct( original_sb,
        reconstructed_mb.V_sub.at( sb_column, sb_row ).contents() );

      frame_sb.mutable_coefficients() = UVBlock::quantize( quantizer, frame_sb.coefficients() );
      frame_sb.calculate_has_nonzero();
    }
  );
}

void Encoder::optimize_interframe_probs( InterFrame & frame )
{
  size_t inter_count  = 0;
  size_t last_count   = 0;
  size_t golden_count = 0;
  size_t total_count  = 0;

  frame.mutable_macroblocks().forall(
    [&] ( InterFrameMacroblock & frame_mb )
    {
      total_count++;

      switch( frame_mb.header().reference() ) {
      case CURRENT_FRAME:
        inter_count++;
        break;

      case LAST_FRAME:
        last_count++;
        break;

      case GOLDEN_FRAME:
        golden_count++;
        break;

      case ALTREF_FRAME:
        break;
      }
    }
  );

  frame.mutable_header().prob_inter = Encoder::calc_prob( inter_count, total_count );
  frame.mutable_header().prob_references_last = Encoder::calc_prob( last_count, total_count );
  frame.mutable_header().prob_references_golden = Encoder::calc_prob( golden_count, total_count );
}

template<>
pair<InterFrame, double> Encoder::encode_with_quantizer<InterFrame>( const VP8Raster & raster,
                                                                     const QuantIndices & quant_indices )
{
  const uint16_t width = raster.display_width();
  const uint16_t height = raster.display_height();

  InterFrame frame = Encoder::make_empty_frame<InterFrame>( width, height );
  frame.mutable_header().quant_indices = quant_indices;

  Quantizer quantizer( frame.header().quant_indices );
  MutableRasterHandle reconstructed_raster_handle { width, height };
  VP8Raster & reconstructed_raster = reconstructed_raster_handle.get();

  TokenBranchCounts token_branch_counts;

  raster.macroblocks().forall_ij(
    [&] ( VP8Raster::Macroblock & original_mb, unsigned int mb_column, unsigned int mb_row )
    {
      auto & reconstructed_mb = reconstructed_raster.macroblock( mb_column, mb_row );
      auto & temp_mb = temp_raster().macroblock( mb_column, mb_row );
      auto & frame_mb = frame.mutable_macroblocks().at( mb_column, mb_row );

      // Process Y and Y2
      luma_mb_inter_predict( original_mb, reconstructed_mb, temp_mb, frame_mb,
                             quantizer, FIRST_PASS );

      if ( frame_mb.inter_coded() ) {
        chroma_mb_inter_predict( original_mb, reconstructed_mb, temp_mb,
                                 frame_mb, quantizer, FIRST_PASS );
      }
      else {
        chroma_mb_intra_predict( original_mb, reconstructed_mb, temp_mb,
                                 frame_mb, quantizer, FIRST_PASS );
      }

      frame_mb.calculate_has_nonzero();

      if ( frame_mb.inter_coded() ) {
        frame_mb.reconstruct_inter( quantizer, references_, reconstructed_mb );
      }
      else {
        frame_mb.reconstruct_intra( quantizer, reconstructed_mb );
      }

      frame_mb.accumulate_token_branches( token_branch_counts );
    }
  );

  frame.relink_y2_blocks();

  optimize_prob_skip( frame );
  optimize_interframe_probs( frame );
  optimize_probability_tables( frame, token_branch_counts );

  apply_best_loopfilter_settings( raster, reconstructed_raster, frame );

  return make_pair( move( frame ), reconstructed_raster.quality( raster ) );
}
