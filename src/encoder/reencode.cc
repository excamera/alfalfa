/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <limits>

#include "encoder.hh"
#include "scorer.hh"

using namespace std;

/*
 * Legend:
 *  --X(Y)-- => Normal frame with output raster X and frame name Y
 *  ==X(Y)== => Switching frame with output raster X and frame name Y
 *         O => Marks a state
 *
 * previous round (1)   -----O-----O--d1--O==D1(F1)==O
 *  current round (2)   -----O-----O--d2--O==D2(F2)==O
 *
 * The goal is to make D1 == D2. We have d1 from the previous run,
 * we have d2 from the current run and also we have the residuals from the
 * frame that is producing D1 (which we call prev_swf-- previous switching frame).
 *
 * Based on the definition of a switching frame, we can say:
 * D1 = idct(deq(q(dct(d1)) + r1))
 * D2 = idct(deq(q(dct(d2)) + r2))
 * To make D1 == D2, we must have:
 * q(dct(d1)) + r1 == q(dct(d2)) + r2
 * => r2 = q(dct(d1)) - q(dct(d2)) + r1
 */

template<class FrameType>
InterFrame Encoder::reencode_frame( const VP8Raster & original_raster,
                                    const FrameType & original_frame )
{
  InterFrame frame = Encoder::make_empty_frame<InterFrame>( width(), height(), true, false );
  auto & kf_header = original_frame.header();
  auto & if_header = frame.mutable_header();

  if ( kf_header.update_segmentation.initialized() ) {
    throw Unsupported( "segmentation not supported" );
  }

  if_header.filter_type             = kf_header.filter_type;
  if_header.update_segmentation     = kf_header.update_segmentation;
  if_header.loop_filter_level       = kf_header.loop_filter_level;
  if_header.sharpness_level         = kf_header.sharpness_level;
  if_header.mode_lf_adjustments     = kf_header.mode_lf_adjustments;
  if_header.quant_indices           = kf_header.quant_indices;
  if_header.refresh_last            = true;
  if_header.refresh_golden_frame    = true;
  if_header.refresh_alternate_frame = true;
  if_header.refresh_entropy_probs   = true;
  if_header.copy_buffer_to_golden.clear();
  if_header.copy_buffer_to_alternate.clear();

  if_header.intra_16x16_prob.clear();
  if_header.intra_16x16_prob.initialize();
  if_header.intra_chroma_prob.clear();
  if_header.intra_chroma_prob.initialize();

  for ( size_t i = 0; i < k_default_y_mode_probs.size(); i++ ) {
    if_header.intra_16x16_prob.get().at( i ) = k_default_y_mode_probs.at( i );
  }

  for ( size_t i = 0; i < k_default_uv_mode_probs.size(); i++ ) {
    if_header.intra_chroma_prob.get().at( i ) = k_default_uv_mode_probs.at( i );
  }

  // for ( unsigned int i = 0; i < BLOCK_TYPES; i++ ) {
  //   for ( unsigned int j = 0; j < COEF_BANDS; j++ ) {
  //     for ( unsigned int k = 0; k < PREV_COEF_CONTEXTS; k++ ) {
  //       for ( unsigned int l = 0; l < ENTROPY_NODES; l++ ) {
  //         TokenProbUpdate & frame_prob = frame.mutable_header().token_prob_update.at( i ).at( j ).at( k ).at( l );
  //         frame_prob = TokenProbUpdate( true, k_default_coeff_probs.at( i ).at( j ).at( k ).at( l ) );
  //       }
  //     }
  //   }
  // }

  Quantizer quantizer( frame.header().quant_indices );
  MutableRasterHandle reconstructed_raster_handle { width(), height() };
  VP8Raster & reconstructed_raster = reconstructed_raster_handle.get();

  MVComponentCounts component_counts;
  TokenBranchCounts token_branch_counts;

  original_raster.macroblocks_forall_ij(
    [&] ( VP8Raster::ConstMacroblock original_mb, unsigned int mb_column, unsigned int mb_row )
    {
      auto reconstructed_mb = reconstructed_raster.macroblock( mb_column, mb_row );
      auto temp_mb = temp_raster().macroblock( mb_column, mb_row );
      auto & frame_mb = frame.mutable_macroblocks().at( mb_column, mb_row );

      // Process Y and Y2
      luma_mb_inter_predict( original_mb.macroblock(), reconstructed_mb, temp_mb, frame_mb,
                             quantizer, component_counts, FIRST_PASS );

      if ( frame_mb.inter_coded() ) {
        chroma_mb_inter_predict( original_mb.macroblock(), reconstructed_mb, temp_mb,
                                 frame_mb, quantizer, FIRST_PASS );
      }
      else {
        chroma_mb_intra_predict( original_mb.macroblock(), reconstructed_mb, temp_mb,
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

  apply_best_loopfilter_settings( original_raster, reconstructed_raster, frame );

  RasterHandle immutable_raster( move( reconstructed_raster_handle ) );
  frame.copy_to( immutable_raster, references_ );

  return frame;
}

void Encoder::update_macroblock( const VP8Raster::Macroblock & original_mb,
                                 VP8Raster::Macroblock & reconstructed_mb,
                                 VP8Raster::Macroblock & temp_mb,
                                 InterFrameMacroblock & frame_mb,
                                 const InterFrameMacroblock & original_fmb,
                                 const Quantizer & quantizer )
{
  frame_mb.mutable_header() = original_fmb.header();

  mbmode luma_pred_mode = original_fmb.y_prediction_mode();

  MotionVector best_mv;

  switch( luma_pred_mode ) {
  case DC_PRED:
  case V_PRED:
  case H_PRED:
  case TM_PRED:
    reconstructed_mb.Y.intra_predict( luma_pred_mode );
    break;

  case B_PRED:
    reconstructed_mb.Y_sub_forall_ij(
      [&] ( VP8Raster::Block4 & reconstructed_sb, unsigned int sb_column, unsigned int sb_row )
      {
        auto & original_sb = original_mb.Y_sub_at( sb_column, sb_row );
        auto & frame_sb = frame_mb.Y().at( sb_column, sb_row );
        bmode sb_prediction_mode = original_fmb.Y().at( sb_column, sb_row ).prediction_mode();

        reconstructed_sb.intra_predict( sb_prediction_mode );

        luma_sb_apply_intra_prediction( original_sb, reconstructed_sb,
                                        frame_sb, quantizer,
                                        sb_prediction_mode, FIRST_PASS );
      }
    );

    break;

  case NEARESTMV:
  case NEARMV:
  case ZEROMV:
  case NEWMV:
  {
    const VP8Raster & reference = references_.at( frame_mb.header().reference() );
    best_mv = original_fmb.base_motion_vector();

    reconstructed_mb.Y.inter_predict( best_mv, reference.Y() );
    break;
  }

  case SPLITMV:
  {
    const VP8Raster & reference = references_.at( frame_mb.header().reference() );
    best_mv = original_fmb.base_motion_vector();
    frame_mb.set_base_motion_vector( best_mv );

    frame_mb.Y().forall_ij(
      [&] ( YBlock & block, const unsigned int column, const unsigned int row )
      {
        block.set_motion_vector( original_fmb.Y().at( column, row ).motion_vector() );
        block.set_Y_without_Y2();
        block.set_prediction_mode( original_fmb.Y().at( column, row ).prediction_mode() );

        reconstructed_mb.Y_sub_at( column, row ).inter_predict( block.motion_vector(), reference.Y() );
      }
    );

    break;
  }

  default:
    throw Unsupported( "unsupported prediction mode" );
  }

  if ( frame_mb.inter_coded() ) {
    luma_mb_apply_inter_prediction( original_mb, reconstructed_mb,
                                    frame_mb, quantizer, luma_pred_mode,
                                    best_mv );

    chroma_mb_inter_predict( original_mb, reconstructed_mb, temp_mb,
                             frame_mb, quantizer, FIRST_PASS );

    frame_mb.calculate_has_nonzero();
    frame_mb.reconstruct_inter( quantizer, references_, reconstructed_mb );
  }
  else {
    luma_mb_apply_intra_prediction( original_mb, reconstructed_mb, temp_mb,
                                    frame_mb, quantizer, luma_pred_mode,
                                    FIRST_PASS );

    mbmode chroma_pred_mode = original_fmb.uv_prediction_mode();

    reconstructed_mb.U.intra_predict( chroma_pred_mode );
    reconstructed_mb.V.intra_predict( chroma_pred_mode );

    chroma_mb_apply_intra_prediction( original_mb, reconstructed_mb, temp_mb,
                                      frame_mb, quantizer, chroma_pred_mode,
                                      FIRST_PASS );

    frame_mb.calculate_has_nonzero();
    frame_mb.reconstruct_intra( quantizer, reconstructed_mb );
  }
}

InterFrame Encoder::update_residues( const VP8Raster & original_raster,
                                     const InterFrame & original_frame )
{
  InterFrame frame = Encoder::make_empty_frame<InterFrame>( width(), height(),
                                                            true, false );
  //frame.mutable_header() = original_frame.header();

  const InterFrameHeader & of_header = original_frame.header();
  InterFrameHeader & if_header = frame.mutable_header();

  if_header.update_segmentation      = of_header.update_segmentation;
  if_header.filter_type              = of_header.filter_type;
  if_header.loop_filter_level        = of_header.loop_filter_level;
  if_header.sharpness_level          = of_header.sharpness_level;
  if_header.mode_lf_adjustments      = of_header.mode_lf_adjustments;
  if_header.quant_indices            = of_header.quant_indices;
  if_header.refresh_last             = of_header.refresh_last;
  if_header.refresh_golden_frame     = of_header.refresh_golden_frame;
  if_header.refresh_alternate_frame  = of_header.refresh_alternate_frame;
  if_header.copy_buffer_to_golden    = of_header.copy_buffer_to_golden;
  if_header.copy_buffer_to_alternate = of_header.copy_buffer_to_alternate;
  if_header.sign_bias_golden         = of_header.sign_bias_golden;
  if_header.sign_bias_alternate      = of_header.sign_bias_alternate;
  if_header.refresh_entropy_probs    = of_header.refresh_entropy_probs;
  if_header.prob_references_last     = of_header.prob_references_last;
  if_header.prob_references_golden   = of_header.prob_references_golden;

  for ( size_t i = 0; i < 2; i++ ) {
    for ( size_t j = 0; j < MV_PROB_CNT; j++ ) {
      if ( not of_header.mv_prob_update.at( i ).at( j ).initialized() ) continue;

      if ( decoder_state_.probability_tables.motion_vector_probs.at( i ).at( j ) !=
           of_header.mv_prob_update.at( i ).at( j ).get() ) {
        if_header.mv_prob_update.at( i ).at( j ) = of_header.mv_prob_update.at( i ).at( j );
      }
    }
  }

  Quantizer quantizer( frame.header().quant_indices );
  MutableRasterHandle reconstructed_raster_handle { width(), height() };
  VP8Raster & reconstructed_raster = reconstructed_raster_handle.get();

  TokenBranchCounts token_branch_counts;

  original_raster.macroblocks_forall_ij(
    [&] ( VP8Raster::ConstMacroblock original_mb, unsigned int mb_column, unsigned int mb_row )
    {
      auto reconstructed_mb = reconstructed_raster.macroblock( mb_column, mb_row );
      auto temp_mb = temp_raster().macroblock( mb_column, mb_row );
      auto & original_fmb = original_frame.macroblocks().at( mb_column, mb_row );
      auto & frame_mb = frame.mutable_macroblocks().at( mb_column, mb_row );

      update_macroblock( original_mb.macroblock(), reconstructed_mb, temp_mb, frame_mb,
                         original_fmb, quantizer );

      frame_mb.calculate_has_nonzero();
      frame_mb.accumulate_token_branches( token_branch_counts );
    }
  );

  frame.relink_y2_blocks();

  optimize_prob_skip( frame );
  optimize_interframe_probs( frame );
  optimize_probability_tables( frame, token_branch_counts );

  frame.loopfilter( decoder_state_.segmentation, decoder_state_.filter_adjustments, reconstructed_raster );

  RasterHandle immutable_raster( move( reconstructed_raster_handle ) );
  frame.copy_to( immutable_raster, references_ );

  return frame;
}

void Encoder::refine_switching_frame( InterFrame & F2, const InterFrame & F1,
                                      const VP8Raster & d1 )
{
  assert( F1.switching_frame() and F2.switching_frame() );
  assert( F2.header().refresh_entropy_probs );
  // switching frame tries to force probabilities convergence, so this must be
  // true.

  const VP8Raster & d2 = references_.at( LAST_FRAME );

  TwoD<uint8_t> blank_block_parent { 4, 4 };
  blank_block_parent.fill( 0 ); /* XXX do we want to do this every time? */
  TwoDSubRange<uint8_t, 4, 4> blank_block { blank_block_parent, 0, 0 };

  Quantizer quantizer( F2.header().quant_indices );

  TokenBranchCounts token_branch_counts;

  F2.macroblocks().forall_ij(
    [&] ( InterFrameMacroblock & F2_mb, unsigned int mb_column, unsigned int mb_row )
    {
      auto & F1_mb = F1.macroblocks().at( mb_column, mb_row );
      auto d1_mb = d1.macroblock( mb_column, mb_row );
      auto d2_mb = d2.macroblock( mb_column, mb_row );

      DCTCoefficients F2_coeffs;
      DCTCoefficients F1_coeffs;

      F2_mb.Y().forall_ij(
        [&] ( YBlock & F2_sb, unsigned int sb_column, unsigned int sb_row )
        {
          auto & F1_sb = F1_mb.Y().at( sb_column, sb_row );
          auto & d1_sb = d1_mb.macroblock().Y_sub_at( sb_column, sb_row );
          auto & d2_sb = d2_mb.macroblock().Y_sub_at( sb_column, sb_row );

          // q(dct(d1))
          F1_coeffs.subtract_dct( d1_sb, blank_block );
          F1_coeffs = F1_coeffs.quantize( quantizer.y() );

          // q(dct(d2))
          F2_coeffs.subtract_dct( d2_sb, blank_block );
          F2_coeffs = F2_coeffs.quantize( quantizer.y() );

          // q(dct(d1)) - q(dct(d2))
          F2_sb.mutable_coefficients() = F1_coeffs - F2_coeffs;

          // r2 = q(dct(d1)) - q(dct(d2)) + r1
          F2_sb.mutable_coefficients() = F2_sb.coefficients() + F1_sb.coefficients();
          F2_sb.calculate_has_nonzero();
        }
      );

      F2_mb.U().forall_ij(
        [&] ( UVBlock & F2_sb, unsigned int sb_column, unsigned int sb_row )
        {
          auto & F1_sb = F1_mb.U().at( sb_column, sb_row );
          auto & d1_sb = d1_mb.macroblock().U_sub_at( sb_column, sb_row );
          auto & d2_sb = d2_mb.macroblock().U_sub_at( sb_column, sb_row );

          // q(dct(d1))
          F1_coeffs.subtract_dct( d1_sb, blank_block );
          F1_coeffs = F1_coeffs.quantize( quantizer.uv() );

          // q(dct(d2))
          F2_coeffs.subtract_dct( d2_sb, blank_block );
          F2_coeffs = F2_coeffs.quantize( quantizer.uv() );

          // q(dct(d1)) - q(dct(d2))
          F2_sb.mutable_coefficients() = F1_coeffs - F2_coeffs;

          // r2 = q(dct(d1)) - q(dct(d2)) + r1
          F2_sb.mutable_coefficients() = F2_sb.coefficients() + F1_sb.coefficients();
          F2_sb.calculate_has_nonzero();
        }
      );

      F2_mb.V().forall_ij(
        [&] ( UVBlock & F2_sb, unsigned int sb_column, unsigned int sb_row )
        {
          auto & F1_sb = F1_mb.V().at( sb_column, sb_row );
          auto & d1_sb = d1_mb.macroblock().V_sub_at( sb_column, sb_row );
          auto & d2_sb = d2_mb.macroblock().V_sub_at( sb_column, sb_row );

          // q(dct(d1))
          F1_coeffs.subtract_dct( d1_sb, blank_block );
          F1_coeffs = F1_coeffs.quantize( quantizer.uv() );

          // q(dct(d2))
          F2_coeffs.subtract_dct( d2_sb, blank_block );
          F2_coeffs = F2_coeffs.quantize( quantizer.uv() );

          // q(dct(d1)) - q(dct(d2))
          F2_sb.mutable_coefficients() = F1_coeffs - F2_coeffs;

          // r2 = q(dct(d1)) - q(dct(d2)) + r1
          F2_sb.mutable_coefficients() = F2_sb.coefficients() + F1_sb.coefficients();
          F2_sb.calculate_has_nonzero();
        }
      );

      F2_mb.calculate_has_nonzero();
      F2_mb.accumulate_token_branches( token_branch_counts );
    }
  );

  F2.relink_y2_blocks();

  optimize_prob_skip( F2 );
  optimize_interframe_probs( F2 );
  optimize_probability_tables( F2, token_branch_counts );
}

InterFrame Encoder::create_switching_frame( const uint8_t y_ac_qi )
{
  InterFrame frame = Encoder::make_empty_frame<InterFrame>( width(), height(), false, true );
  auto & if_header = frame.mutable_header();

  QuantIndices quant_indices;
  quant_indices.y_ac_qi = y_ac_qi;

  if_header.loop_filter_level = 0;
  if_header.mode_lf_adjustments.clear();
  if_header.quant_indices = quant_indices;
  if_header.refresh_last = true;
  if_header.refresh_entropy_probs = true;

  for ( size_t i = 0; i < 2; i++ ) {
    for ( size_t j = 0; j < MV_PROB_CNT; j++ ) {
      uint8_t current_prob = decoder_state_.probability_tables.motion_vector_probs.at( i ).at( j );
      if_header.mv_prob_update.at( i ).at( j ) = MVProbUpdate( true, MVProbUpdate::read_half_prob( MVProbUpdate::write_prob( current_prob ) ) );
    }
  }

  if_header.intra_16x16_prob.clear();
  if_header.intra_chroma_prob.clear();

  TokenBranchCounts token_branch_counts;

  frame.mutable_macroblocks().forall(
    [&] ( InterFrameMacroblock & frame_mb )
    {
      frame_mb.mutable_header().is_inter_mb = true;
      frame_mb.mutable_header().set_reference( LAST_FRAME );

      frame_mb.Y2().set_prediction_mode( ZEROMV );
      frame_mb.Y2().set_coded( false );
      frame_mb.Y2().zero_out();

      frame_mb.set_base_motion_vector( MotionVector() );

      frame_mb.Y().forall(
        [&] ( YBlock & frame_sb ) {
          frame_sb.set_Y_without_Y2();
          frame_sb.set_motion_vector( MotionVector() );
          frame_sb.zero_out(); // no residuals
        }
      );

      frame_mb.U().forall( [&] ( UVBlock & frame_sb ) { frame_sb.zero_out(); } );
      frame_mb.V().forall( [&] ( UVBlock & frame_sb ) { frame_sb.zero_out(); } );

      frame_mb.calculate_has_nonzero();
      frame_mb.accumulate_token_branches( token_branch_counts );
    }
  );

  frame.relink_y2_blocks();

  optimize_prob_skip( frame );
  optimize_interframe_probs( frame );
  optimize_probability_tables( frame, token_branch_counts );

  return frame;
}

void Encoder::fix_probability_tables( InterFrame & frame,
                                      const ProbabilityTables & target )
{
  for ( unsigned int i = 0; i < BLOCK_TYPES; i++ ) {
    for ( unsigned int j = 0; j < COEF_BANDS; j++ ) {
      for ( unsigned int k = 0; k < PREV_COEF_CONTEXTS; k++ ) {
        for ( unsigned int l = 0; l < ENTROPY_NODES; l++ ) {
          uint8_t current_prob = decoder_state_.probability_tables.coeff_probs.at( i ).at( j ).at( k ).at( l );
          uint8_t target_prob = target.coeff_probs.at( i ).at( j ).at( k ).at( l );
          TokenProbUpdate & frame_prob = frame.mutable_header().token_prob_update.at( i ).at( j ).at( k ).at( l );

          if ( target_prob != current_prob ) {
            frame_prob = TokenProbUpdate( true, target_prob );
          }
          else {
            frame_prob = TokenProbUpdate();
          }
        }
      }
    }
  }
}

void Encoder::fix_mv_probabilities( InterFrame & frame,
                                    const ProbabilityTables & target )
{
  for ( size_t i = 0; i < 2; i++ ) {
    for ( size_t j = 0; j < MV_PROB_CNT; j++ ) {
      uint8_t current_prob = decoder_state_.probability_tables.motion_vector_probs.at( i ).at( j );
      uint8_t target_prob = target.motion_vector_probs.at( i ).at( j );

      if ( current_prob != target_prob ) {
        frame.mutable_header().mv_prob_update.at( i ).at( j ) = MVProbUpdate( true, target_prob );
      }
    }
  }
}

void Encoder::reencode( FrameInput & input, const IVF & pred_ivf,
                        Decoder pred_decoder, const uint8_t s_ac_qi,
                        const bool refine_sw, const bool fix_prob_tables )
{
  if ( input.display_width() != width() or input.display_height() != height()
       or pred_ivf.width() != width() or pred_ivf.height() != height() ) {
    throw Unsupported( "scaling not supported" );
  }

  Decoder input_decoder( width(), height() );

  size_t frame_index = 0;
  Optional<RasterHandle> raster;

  for ( frame_index = 0, raster = input.get_next_frame();
        raster.initialized();
        frame_index++, raster = input.get_next_frame()) {
    cerr << "Re-encoding Frame #" << frame_index << "..." << endl;

    const VP8Raster & target_output = raster.get();
    UncompressedChunk pred_uch { pred_ivf.frame( frame_index ), width(), height() };

    if ( not pred_uch.show_frame() ) {
      throw Unsupported( "unshown frame in the prediction ivf" );
    }

    if ( pred_uch.key_frame() ) {
      KeyFrame frame = pred_decoder.parse_frame<KeyFrame>( pred_uch );
      pred_decoder.decode_frame( frame );

      write_frame( reencode_frame( target_output, frame ) );
    }
    else {
      InterFrame frame = pred_decoder.parse_frame<InterFrame>( pred_uch );
      pred_decoder.decode_frame( frame );

      if ( frame_index == 0 ) {
        write_frame( reencode_frame( target_output, frame ) );
      }
      else {
        write_frame( update_residues( target_output, frame ) );
      }
    }
  }

  if ( s_ac_qi != numeric_limits<uint8_t>::max() ) {
    if ( s_ac_qi > 127 ) {
      throw runtime_error( "s_ac_qi should be less than or equal to 127" );
    }

    InterFrame switching_frame = create_switching_frame( s_ac_qi );

    if ( refine_sw and frame_index == pred_ivf.frame_count() - 1 ) {
      /* Variable names are explained in refine_switching_frame method */
      RasterHandle d1 = pred_decoder.get_references().last;

      UncompressedChunk pred_uch { pred_ivf.frame( frame_index ), width(), height() };
      InterFrame F1 = pred_decoder.parse_frame<InterFrame>( pred_uch );
      pred_decoder.decode_frame( F1 );

      if ( F1.header().quant_indices.y_ac_qi != s_ac_qi ) {
        throw runtime_error( "s_ac_qi mismatch" );
      }

      refine_switching_frame( switching_frame, F1, d1 );

      if ( fix_prob_tables ) {
        fix_probability_tables( switching_frame, pred_decoder.get_state().probability_tables );
        fix_mv_probabilities( switching_frame, pred_decoder.get_state().probability_tables );
      }
    }

    write_switching_frame( switching_frame );
  }
}

void Encoder::write_switching_frame( const InterFrame & frame )
{
  assert( frame.switching_frame() );

  MutableRasterHandle reconstructed_raster_handle { width(), height() };
  VP8Raster & reconstructed_raster = reconstructed_raster_handle.get();

  write_frame( frame );

  frame.decode( { }, references_, reconstructed_raster );
  frame.copy_to( move( reconstructed_raster_handle ), references_ );
}
