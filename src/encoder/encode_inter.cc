/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <limits>

#include "encoder.hh"
#include "scorer.hh"

using namespace std;

static bool out_of_bounds( const MotionVector & mv )
{
  int16_t x = mv.x();
  int16_t y = mv.y();

  if ( x >  1023 ) return true;
  if ( x < -1023 ) return true;
  if ( y >  1023 ) return true;
  if ( y < -1023 ) return true;

  return false;
}

void Encoder::update_mv_component_counts( const int16_t & component,
                                          const bool is_x,
                                          Encoder::MVComponentCounts & counts ) const
{
  enum { IS_SHORT, SIGN, SHORT, BITS = SHORT + 8 - 1, LONG_WIDTH = 10 };

  uint16_t x = abs( component );
  uint8_t comp_idx = is_x ? 1 : 0;

  if ( x < 8 ) {
    counts.at( comp_idx ).at( IS_SHORT ).first++;

    if ( x < 4 ) {
      counts.at( comp_idx ).at( SHORT + 0 ).first++;

      if ( x < 2 ) {
        counts.at( comp_idx ).at( SHORT + 1 ).first++;

        if ( x == 0 ) {
          counts.at( comp_idx ).at( SHORT + 2 ).first++;
        }
        else { // x == 1
          counts.at( comp_idx ).at( SHORT + 2 ).second++;
        }
      }
      else { /* x >= 2 */
        counts.at( comp_idx ).at( SHORT + 1 ).second++;

        if ( x == 2 ) {
          counts.at( comp_idx ).at( SHORT + 3 ).first++;
        }
        else { // x == 3
          counts.at( comp_idx ).at( SHORT + 3 ).second++;
        }
      }
    }
    else { /* x >= 4 */
      counts.at( comp_idx ).at( SHORT + 0 ).second++;

      if ( x < 6 ) {
        counts.at( comp_idx ).at( SHORT + 4 ).first++;

        if ( x == 4 ) {
          counts.at( comp_idx ).at( SHORT + 5 ).first++;
        }
        else { // x == 5
          counts.at( comp_idx ).at( SHORT + 5 ).second++;
        }
      }
      else { /* x >= 6 */
        counts.at( comp_idx ).at( SHORT + 4 ).second++;

        if ( x == 6 ) {
          counts.at( comp_idx ).at( SHORT + 6 ).first++;
        }
        else { // x == 7
          counts.at( comp_idx ).at( SHORT + 6 ).second++;
        }
      }
    }
  }
  else { /* x >= 8 */
    counts.at( comp_idx ).at( IS_SHORT ).second++;

    for ( uint8_t i = 0; i < LONG_WIDTH; i++ ) {
      if ( i == 3 and x & 0xfff0 ) {
        if ( ( x >> 3 ) & 1 ) {
          counts.at( comp_idx ).at( BITS + i ).second++;
        }
        else {
          counts.at( comp_idx ).at( BITS + i ).first++;
        }
      }
      else if ( i != 3 ) {
        if ( (x >> i) & 1 ) {
          counts.at( comp_idx ).at( BITS + i ).second++;
        }
        else {
          counts.at( comp_idx ).at( BITS + i ).first++;
        }
      }
    }
  }

  if ( component < 0 ) {
    counts.at( comp_idx ).at( SIGN ).second++;
  }
  else {
    counts.at( comp_idx ).at( SIGN ).first++;
  }
}

/*
 * Taken from: libvpx:vp8/encoder/rdopt.c:135
 */
static const array<int, 128> sad_per_bit16lut = {
  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3,
  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,
  4,  4,  4,  4,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  6,  6,  6,
  6,  6,  6,  6,  6,  6,  6,  6,  6,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
  7,  7,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  9,  9,  9,  9,  9,
  9,  9,  9,  9,  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11,
  11, 11, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14
};

MotionVector Encoder::diamond_search( const VP8Raster::Macroblock & original_mb,
                                      VP8Raster::Macroblock & reconstructed_mb,
                                      VP8Raster::Macroblock & temp_mb,
                                      InterFrameMacroblock & frame_mb,
                                      const VP8Raster & reference,
                                      MotionVector base_mv,
                                      MotionVector origin,
                                      size_t step_size ) const
{
  TwoDSubRange<uint8_t, 16, 16> & prediction = temp_mb.Y.mutable_contents();

  base_mv = Scorer::clamp( base_mv, frame_mb.context() );

  while ( step_size > 1 ) {
    MBPredictionData best_pred;
    MBPredictionData pred;

    for ( int x = -1; x <= 1; x++ ) {
      for ( int y = -1; y <= 1; y++ ) {
        if ( ( ( x & y ) == 0 ) && ( ( x | y ) != 0 ) ) continue;

        pred.mv = MotionVector();
        MotionVector direction( ( ( step_size ) * ( ( x + y ) / 2 ) ),
                                ( ( step_size ) * ( ( x - y ) / 2 ) ) );

        pred.mv += origin + direction;

        if ( out_of_bounds( pred.mv ) ) continue;

        MotionVector this_mv( Scorer::clamp( pred.mv + base_mv, frame_mb.context() ) );

        reconstructed_mb.Y.inter_predict( this_mv, reference.Y(), prediction );
        pred.distortion = sad( original_mb.Y, prediction );
        pred.rate = costs_.sad_motion_vector_cost( pred.mv, MotionVector(), sad_per_bit16lut[ qindex_ ] );
        pred.cost = rdcost( pred.rate, pred.distortion, 1, 1 );

        if ( pred.cost < best_pred.cost  ) {
          best_pred = pred;
        }
      }
    }

    origin = best_pred.mv;
    step_size /= 2;
  }

  return origin;
}

void Encoder::luma_mb_inter_predict( const VP8Raster::Macroblock & original_mb,
                                     VP8Raster::Macroblock & reconstructed_mb,
                                     VP8Raster::Macroblock & temp_mb,
                                     InterFrameMacroblock & frame_mb,
                                     const Quantizer & quantizer,
                                     MVComponentCounts & /* component_counts */,
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

  /* MBPredictionData best_uv_pred = chroma_mb_best_prediction_mode( original_mb,
                                                                  reconstructed_mb,
                                                                  temp_mb,
                                                                  true );

  best_pred.distortion += best_uv_pred.distortion;
  best_pred.rate       += best_uv_pred.rate;
  best_pred.cost       += best_uv_pred.cost; */

  MotionVector best_mv;
  const VP8Raster & reference = references_.last.get();
  const auto & reference_mb = reference.macroblock( frame_mb.context().column,
                                                    frame_mb.context().row );

  TwoDSubRange<uint8_t, 16, 16> & prediction = temp_mb.Y.mutable_contents();

  const Scorer census = frame_mb.motion_vector_census();
  const MotionVector best_ref = Scorer::clamp( census.best(), frame_mb.context() );
  const auto counts = census.mode_contexts();
  const ProbabilityArray< num_mv_refs > mv_ref_probs = {{ mv_counts_to_probs.at( counts.at( 0 ) ).at( 0 ),
                                                          mv_counts_to_probs.at( counts.at( 1 ) ).at( 1 ),
                                                          mv_counts_to_probs.at( counts.at( 2 ) ).at( 2 ),
                                                          mv_counts_to_probs.at( counts.at( 3 ) ).at( 3 ) }};

  costs_.fill_mv_ref_costs( mv_ref_probs );
  costs_.fill_mv_component_costs( decoder_state_.probability_tables.motion_vector_probs );
  costs_.fill_mv_sad_costs();

  constexpr array<mbmode, 4> inter_modes = { ZEROMV, NEARESTMV, NEARMV, NEWMV, /* SPLIMV */ };

  for ( const mbmode prediction_mode : inter_modes ) {
    MBPredictionData pred;
    pred.prediction_mode = prediction_mode;
    MotionVector mv;

    switch ( prediction_mode ) {
    case NEWMV:
      for ( size_t step = 9; step > 0; step-- ) {
        mv = diamond_search( original_mb, reconstructed_mb, temp_mb, frame_mb,
                             reference, best_ref, mv, ( 1 << step ) );
      }

      mv += best_ref;

      if ( mv.empty() ) {
        continue;
      }

      break;

    case NEARESTMV:
    case NEARMV:
      mv = Scorer::clamp( ( prediction_mode == NEARMV ) ? census.near() : census.nearest(),
                          frame_mb.context() );

      if ( mv.empty() ) {
        // Same as ZEROMV
        continue;
      }

      break;

    case ZEROMV:
      mv = MotionVector();
      break;

    default:
      throw runtime_error( "not supported" );
    }

    reference_mb.Y.inter_predict( mv, reference.Y(), prediction );

    pred.distortion = variance( original_mb.Y, prediction );
    pred.rate = costs_.mbmode_costs.at( 1 ).at( prediction_mode );

    if ( prediction_mode == NEWMV ) {
      pred.rate += costs_.motion_vector_cost( mv - best_ref, 96 );
    }

    /* chroma_mb_inter_predict( original_mb, reconstructed_mb, temp_mb, frame_mb,
                             quantizer, encoder_pass );

    pred.distortion += sse( original_mb.U, reconstructed_mb.U.contents() );
    pred.distortion += sse( original_mb.V, reconstructed_mb.V.contents() ); */

    pred.cost = rdcost( pred.rate, pred.distortion, RATE_MULTIPLIER,
                        DISTORTION_MULTIPLIER );

    if ( pred.cost < best_pred.cost ) {
      best_mv = mv;
      best_pred = pred;
      reconstructed_mb.Y.mutable_contents().copy_from( prediction );
    }
  }

  luma_mb_apply_inter_prediction( original_mb, reconstructed_mb, temp_mb,
                                  frame_mb, quantizer, best_pred.prediction_mode,
                                  best_mv, encoder_pass );
}

void Encoder::luma_mb_apply_inter_prediction( const VP8Raster::Macroblock & original_mb,
                                              VP8Raster::Macroblock & reconstructed_mb,
                                              VP8Raster::Macroblock & temp_mb,
                                              InterFrameMacroblock & frame_mb,
                                              const Quantizer & quantizer,
                                              const mbmode best_pred,
                                              const MotionVector best_mv,
                                              const EncoderPass encoder_pass )
{
  if ( best_pred <= B_PRED ) {
    // This block will be intra-predicted
    frame_mb.mutable_header().is_inter_mb = false;

    luma_mb_apply_intra_prediction( original_mb, reconstructed_mb, temp_mb,
                                    frame_mb, quantizer, best_pred, encoder_pass );
  }
  else {
    // This block will be inter-predicted
    frame_mb.mutable_header().is_inter_mb = true;
    frame_mb.mutable_header().set_reference( LAST_FRAME );

    frame_mb.Y2().set_prediction_mode( best_pred );

    frame_mb.set_base_motion_vector( best_mv );

    frame_mb.Y().forall(
      [&] ( YBlock & frame_sb ) { frame_sb.set_motion_vector( frame_mb.base_motion_vector() ); }
    );

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

    // update_mv_component_counts( ( best_mv - best_ref ).x(), true, component_counts );
    // update_mv_component_counts( ( best_mv - best_ref ).y(), false, component_counts );
  }
}

void Encoder::chroma_mb_inter_predict( const VP8Raster::Macroblock & original_mb,
                                       VP8Raster::Macroblock & reconstructed_mb,
                                       VP8Raster::Macroblock & temp_mb,
                                       InterFrameMacroblock & frame_mb,
                                       const Quantizer & quantizer,
                                       const EncoderPass ) const
{
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

void Encoder::optimize_mv_probs( InterFrame & frame, const MVComponentCounts & counts )
{
  for ( size_t i = 0; i < 2; i++ ) {
    for ( size_t j = 0; j < MV_PROB_CNT; j++) {
      const uint32_t false_count = counts.at( i ).at( j ).first;
      const uint32_t true_count = counts.at( i ).at( j ).second;

      const uint32_t prob = Encoder::calc_prob( false_count, false_count + true_count );

      if ( prob > 1 and prob != decoder_state_.probability_tables.motion_vector_probs.at( i ).at( j ) ) {
        frame.mutable_header().mv_prob_update.at( i ).at( j ) = MVProbUpdate( true, ( prob >> 1 ) << 1 );
      }
    }
  }

  decoder_state_.probability_tables.mv_prob_update( frame.header().mv_prob_update );
}

void Encoder::optimize_interframe_probs( InterFrame & frame )
{
  array<pair<uint32_t, uint32_t>, 3> probs;
  size_t total_count = 0;

  frame.macroblocks().forall(
    [&] ( const InterFrameMacroblock & frame_mb )
    {
      total_count++;

      switch( frame_mb.header().reference() ) {
      case CURRENT_FRAME: // 0
        probs[ 0 ].first++;
        break;

      case LAST_FRAME: // 10
        probs[ 0 ].second++;
        probs[ 1 ].first++;
        break;

      case GOLDEN_FRAME: // 110
        probs[ 0 ].second++;
        probs[ 1 ].second++;
        probs[ 2 ].first++;
        break;

      case ALTREF_FRAME: // 111
        probs[ 0 ].second++;
        probs[ 1 ].second++;
        probs[ 2 ].second++;
        break;
      }
    }
  );

  uint8_t prob_inter = Encoder::calc_prob( probs[ 0 ].first, probs[ 0 ].first + probs[ 0 ].second );
  uint8_t prob_last  = Encoder::calc_prob( probs[ 1 ].first, probs[ 1 ].first + probs[ 1 ].second );
  uint8_t prob_gf    = Encoder::calc_prob( probs[ 2 ].first, probs[ 2 ].first + probs[ 2 ].second );

  if ( prob_inter > 0 ) {
    frame.mutable_header().prob_inter = prob_inter;
  }

  if ( prob_last > 0 ) {
    frame.mutable_header().prob_references_last = prob_last;
  }

  if ( prob_gf > 0 ) {
    frame.mutable_header().prob_references_golden = prob_gf;
  }
}

template<>
pair<InterFrame, double> Encoder::encode_with_quantizer<InterFrame>( const VP8Raster & raster,
                                                                     const QuantIndices & quant_indices,
                                                                     const bool update_state )
{
  DecoderState decoder_state_copy = decoder_state_;

  InterFrame frame = Encoder::make_empty_frame<InterFrame>( width(), height() );
  frame.mutable_header().quant_indices = quant_indices;
  frame.mutable_header().refresh_entropy_probs = true;
  frame.mutable_header().refresh_last = true;

  Quantizer quantizer( frame.header().quant_indices );
  MutableRasterHandle reconstructed_raster_handle { width(), height() };

  costs_.fill_token_costs( ProbabilityTables() );

  TokenBranchCounts token_branch_counts;
  MVComponentCounts component_counts;

  raster.macroblocks().forall_ij(
    [&] ( VP8Raster::Macroblock & original_mb, unsigned int mb_column, unsigned int mb_row )
    {
      auto & reconstructed_mb = reconstructed_raster_handle.get().macroblock( mb_column, mb_row );
      auto & temp_mb = temp_raster().macroblock( mb_column, mb_row );
      auto & frame_mb = frame.mutable_macroblocks().at( mb_column, mb_row );

      // Process Y and Y2
      luma_mb_inter_predict( original_mb, reconstructed_mb, temp_mb, frame_mb,
                             quantizer, component_counts, FIRST_PASS );

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
        frame_mb.reconstruct_inter( quantizer, references_, reconstructed_mb, false );
      }
      else {
        frame_mb.reconstruct_intra( quantizer, reconstructed_mb );
      }

      frame_mb.accumulate_token_branches( token_branch_counts );
    }
  );

  frame.relink_y2_blocks();

  optimize_prob_skip( frame );
  // optimize_mv_probs( frame, component_counts );
  optimize_interframe_probs( frame );
  optimize_probability_tables( frame, token_branch_counts );
  apply_best_loopfilter_settings( raster, reconstructed_raster_handle.get(), frame );

  RasterHandle immutable_raster( move( reconstructed_raster_handle ) );

  if ( not update_state ) {
    decoder_state_ = decoder_state_copy;
  }
  else {
    references_.last = immutable_raster;
  }

  return { move( frame ), immutable_raster.get().quality( raster ) };
}

/*
 * Legend:
 *  --X(Y)-- => Normal frame with output raster X and frame name Y
 *  ==X(Y)== => Switching frame with output raster X and frame name Y
 *      O => Marks a state
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

void Encoder::refine_switching_frame( InterFrame & F2, const InterFrame & F1,
                                      const VP8Raster & d1 )
{
  assert( F1.switching_frame() and F2.switching_frame() );

  const VP8Raster & d2 = references_.at( LAST_FRAME );

  TwoD<uint8_t> blank_block_parent { 4, 4 };
  blank_block_parent.fill( 0 ); /* XXX do we want to do this every time? */
  TwoDSubRange<uint8_t, 4, 4> blank_block { blank_block_parent, 0, 0 };

  Quantizer quantizer( F2.header().quant_indices );

  F2.macroblocks().forall_ij(
    [&] ( InterFrameMacroblock & F2_mb, unsigned int mb_column, unsigned int mb_row )
    {
      auto & F1_mb = F1.macroblocks().at( mb_column, mb_row );
      auto & d1_mb = d1.macroblock( mb_column, mb_row );
      auto & d2_mb = d2.macroblock( mb_column, mb_row );

      DCTCoefficients F2_coeffs;
      DCTCoefficients F1_coeffs;

      F2_mb.Y().forall_ij(
        [&] ( YBlock & F2_sb, unsigned int sb_column, unsigned int sb_row )
        {
          auto & F1_sb = F1_mb.Y().at( sb_column, sb_row );
          auto & d1_sb = d1_mb.Y_sub.at( sb_column, sb_row );
          auto & d2_sb = d2_mb.Y_sub.at( sb_column, sb_row );

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
          auto & d1_sb = d1_mb.U_sub.at( sb_column, sb_row );
          auto & d2_sb = d2_mb.U_sub.at( sb_column, sb_row );

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
          auto & d1_sb = d1_mb.V_sub.at( sb_column, sb_row );
          auto & d2_sb = d2_mb.V_sub.at( sb_column, sb_row );

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
    }
  );

  optimize_prob_skip( F2 );
  optimize_interframe_probs( F2 );
}

InterFrame Encoder::create_switching_frame( const uint8_t y_ac_qi )
{
  InterFrame frame = Encoder::make_empty_frame<InterFrame>( width(), height() );
  auto & if_header = frame.mutable_header();

  frame.set_switching( true );
  frame.set_show( false );

  QuantIndices quant_indices;
  quant_indices.y_ac_qi = y_ac_qi;

  if_header.loop_filter_level = 0; // disable loopfilter for this if_header
  if_header.mode_lf_adjustments.clear();
  if_header.quant_indices = quant_indices;
  if_header.refresh_last = true;
  if_header.refresh_entropy_probs = false;

  for ( size_t i = 0; i < MV_PROB_CNT; i++ ) {
    if_header.mv_prob_update.at( 0 ).at( i ).clear();
    if_header.mv_prob_update.at( 1 ).at( i ).clear();
  }

  if_header.intra_16x16_prob.clear();

  frame.macroblocks().forall(
    [&] ( InterFrameMacroblock & frame_mb )
    {
      frame_mb.mutable_header().is_inter_mb = true;
      frame_mb.mutable_header().set_reference( LAST_FRAME );

      frame_mb.Y2().set_prediction_mode( SPLITMV );
      frame_mb.Y2().set_if_coded();
      frame_mb.Y2().mutable_coefficients().zero_out();

      frame_mb.set_base_motion_vector( MotionVector() );

      frame_mb.Y().forall(
        [&] ( YBlock & frame_sb ) {
          frame_sb.set_Y_without_Y2();
          frame_sb.set_prediction_mode( ZERO4X4 );
          frame_sb.set_motion_vector( MotionVector() );
          frame_sb.mutable_coefficients().zero_out(); // no residuals
        }
      );

      frame_mb.U().forall(
        [&] ( UVBlock & frame_sb )
        {
          frame_sb.mutable_coefficients().zero_out(); // no residuals
        }
      );

      frame_mb.V().forall(
        [&] ( UVBlock & frame_sb )
        {
          frame_sb.mutable_coefficients().zero_out(); // no residuals
        }
      );

      frame_mb.calculate_has_nonzero();
    }
  );

  optimize_prob_skip( frame );
  optimize_interframe_probs( frame );

  return frame;
}

template<>
void Encoder::reencode_frame( const VP8Raster & unfiltered_output,
                              const KeyFrame & original_frame )
{
  InterFrame frame = Encoder::make_empty_frame<InterFrame>( width(), height() );
  auto & kf_header = original_frame.header();
  auto & if_header = frame.mutable_header();

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

  for ( size_t i = 0; i < MV_PROB_CNT; i++ ) {
    if_header.mv_prob_update.at( 0 ).at( i ).clear();
    if_header.mv_prob_update.at( 1 ).at( i ).clear();
  }

  // TODO MV probabilities.

  if_header.intra_16x16_prob.clear();

  for ( unsigned int i = 0; i < BLOCK_TYPES; i++ ) {
    for ( unsigned int j = 0; j < COEF_BANDS; j++ ) {
      for ( unsigned int k = 0; k < PREV_COEF_CONTEXTS; k++ ) {
        for ( unsigned int l = 0; l < ENTROPY_NODES; l++ ) {
          auto & current_prob = decoder_state_.probability_tables.coeff_probs.at( i ).at( j ).at( k ).at( l );
          auto & kf_header_prob = kf_header.token_prob_update.at( i ).at( j ).at( k ).at( l ).coeff_prob;
          auto & default_kf_prob = k_default_coeff_probs.at( i ).at( j ).at( k ).at( l );

          if ( kf_header_prob.initialized() ) {
            if( current_prob != kf_header_prob.get() ) {
              if_header.token_prob_update.at( i ).at( j ).at( k ).at( l ) = TokenProbUpdate( true, kf_header_prob.get() );
            }
            else {
              if_header.token_prob_update.at( i ).at( j ).at( k ).at( l ).coeff_prob.clear();
            }
          }
          else {
            if ( current_prob != default_kf_prob ) {
              if_header.token_prob_update.at( i ).at( j ).at( k ).at( l ) = TokenProbUpdate( true, default_kf_prob );
            }
            else {
              if_header.token_prob_update.at( i ).at( j ).at( k ).at( l ).coeff_prob.clear();
            }
          }
        }
      }
    }
  }

  decoder_state_.probability_tables.coeff_prob_update( frame.header() );

  Quantizer quantizer( frame.header().quant_indices );
  MutableRasterHandle reconstructed_raster_handle { width(), height() };
  VP8Raster & reconstructed_raster = reconstructed_raster_handle.get();

  MVComponentCounts component_counts;

  unfiltered_output.macroblocks().forall_ij(
    [&] ( VP8Raster::Macroblock & original_mb, unsigned int mb_column, unsigned int mb_row )
    {
      auto & reconstructed_mb = reconstructed_raster.macroblock( mb_column, mb_row );
      auto & temp_mb = temp_raster().macroblock( mb_column, mb_row );
      auto & frame_mb = frame.mutable_macroblocks().at( mb_column, mb_row );

      // Process Y and Y2
      luma_mb_inter_predict( original_mb, reconstructed_mb, temp_mb, frame_mb,
                             quantizer, component_counts, FIRST_PASS );

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
        frame_mb.reconstruct_inter( quantizer, references_, reconstructed_mb, false );
      }
      else {
        frame_mb.reconstruct_intra( quantizer, reconstructed_mb );
      }
    }
  );

  frame.relink_y2_blocks();

  optimize_prob_skip( frame );
  optimize_interframe_probs( frame );

  decoder_state_.filter_adjustments.clear();
  decoder_state_.filter_adjustments.initialize( frame.header() );
  frame.loopfilter( decoder_state_.segmentation, decoder_state_.filter_adjustments, reconstructed_raster );

  RasterHandle immutable_raster( move( reconstructed_raster_handle ) );
  references_.last = immutable_raster;

  ivf_writer_.append_frame( frame.serialize( decoder_state_.probability_tables ) );
}

template<>
void Encoder::reencode_frame( const VP8Raster & unfiltered_output,
                              const InterFrame & original_frame )
{
  InterFrame frame = Encoder::make_empty_frame<InterFrame>( width(), height() );
  frame.mutable_header() = original_frame.header();

  Quantizer quantizer( frame.header().quant_indices );
  MutableRasterHandle reconstructed_raster_handle { width(), height() };
  VP8Raster & reconstructed_raster = reconstructed_raster_handle.get();

  TokenBranchCounts token_branch_counts;

  unfiltered_output.macroblocks().forall_ij(
    [&] ( VP8Raster::Macroblock & original_mb, unsigned int mb_column, unsigned int mb_row )
    {
      auto & reconstructed_mb = reconstructed_raster.macroblock( mb_column, mb_row );
      auto & temp_mb = temp_raster().macroblock( mb_column, mb_row );
      auto & original_frame_mb = original_frame.macroblocks().at( mb_column, mb_row );
      auto & frame_mb = frame.mutable_macroblocks().at( mb_column, mb_row );

      frame_mb.mutable_header() = original_frame_mb.header();

      mbmode luma_pred_mode = original_frame_mb.y_prediction_mode();

      MotionVector best_mv;

      if ( luma_pred_mode < B_PRED ) {
        reconstructed_mb.Y.intra_predict( luma_pred_mode );
      }
      else if ( luma_pred_mode == B_PRED ) {
        reconstructed_mb.Y_sub.forall_ij(
          [&] ( VP8Raster::Block4 & reconstructed_sb, unsigned int sb_column, unsigned int sb_row )
          {
            auto & original_sb = original_mb.Y_sub.at( sb_column, sb_row );
            auto & original_frame_sb = original_frame_mb.Y().at( sb_column, sb_row );
            auto & frame_sb = frame_mb.Y().at( sb_column, sb_row );
            bmode sb_prediction_mode = original_frame_sb.prediction_mode();

            reconstructed_sb.intra_predict( sb_prediction_mode );
            frame_sb.set_prediction_mode( sb_prediction_mode );

            frame_sb.mutable_coefficients().subtract_dct( original_sb,
              reconstructed_sb.contents() );
            frame_sb.mutable_coefficients() = YBlock::quantize( quantizer, frame_sb.coefficients() );

            frame_sb.set_Y_without_Y2();
            frame_sb.calculate_has_nonzero();

            reconstructed_sb.intra_predict( sb_prediction_mode );
            frame_sb.dequantize( quantizer ).idct_add( reconstructed_sb );
          }
        );
      }
      else {
        best_mv = original_frame_mb.base_motion_vector();
        reconstructed_mb.Y.inter_predict( best_mv, references_.last.get().Y() );
      }

      luma_mb_apply_inter_prediction( original_mb, reconstructed_mb, temp_mb,
                                      frame_mb, quantizer, luma_pred_mode,
                                      best_mv, FIRST_PASS );

      if ( frame_mb.inter_coded() ) {
        chroma_mb_inter_predict( original_mb, reconstructed_mb, temp_mb,
                                 frame_mb, quantizer, FIRST_PASS );
      }
      else {
        mbmode chroma_pred_mode = original_frame_mb.uv_prediction_mode();

        reconstructed_mb.U.intra_predict( chroma_pred_mode );
        reconstructed_mb.V.intra_predict( chroma_pred_mode );

        chroma_mb_apply_intra_prediction( original_mb, reconstructed_mb, temp_mb,
                                          frame_mb, quantizer, chroma_pred_mode,
                                          FIRST_PASS );
      }

      frame_mb.calculate_has_nonzero();

      if ( frame_mb.inter_coded() ) {
        frame_mb.reconstruct_inter( quantizer, references_, reconstructed_mb, false );
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

  decoder_state_.filter_adjustments.clear();
  decoder_state_.filter_adjustments.initialize( frame.header() );
  frame.loopfilter( decoder_state_.segmentation, decoder_state_.filter_adjustments, reconstructed_raster );

  RasterHandle immutable_raster( move( reconstructed_raster_handle ) );
  references_.last = immutable_raster;

  ivf_writer_.append_frame( frame.serialize( decoder_state_.probability_tables ) );
}

void Encoder::reencode( FrameInput & input, const IVF & pred_ivf,
                        Decoder pred_decoder, const uint8_t s_ac_qi )
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
      reencode_frame( target_output, frame );
    }
    else {
      InterFrame frame = pred_decoder.parse_frame<InterFrame>( pred_uch );
      pred_decoder.decode_frame( frame );
      reencode_frame( target_output, frame );
    }
  }

  if ( s_ac_qi != numeric_limits<uint8_t>::max() ) {
    if ( s_ac_qi > 127 ) {
      throw runtime_error( "s_ac_qi should be less than or equal to 127" );
    }

    InterFrame switching_frame = create_switching_frame( s_ac_qi );

    if ( frame_index == pred_ivf.frame_count() - 1 ) {
      /* Variable names are explained in refine_switching_frame method */
      RasterHandle d1 = pred_decoder.get_references().last;

      UncompressedChunk pred_uch { pred_ivf.frame( frame_index ), width(), height() };
      InterFrame F1 = pred_decoder.parse_frame<InterFrame>( pred_uch );
      pred_decoder.decode_frame( F1 );

      if ( F1.header().quant_indices.y_ac_qi != s_ac_qi ) {
        throw runtime_error( "s_ac_qi mismatch" );
      }

      refine_switching_frame( switching_frame, F1, d1 );
    }

    write_switching_frame( switching_frame );
  }
}

void Encoder::write_switching_frame( const InterFrame & frame )
{
  assert( frame.switching_frame() );

  MutableRasterHandle reconstructed_raster_handle { width(), height() };
  VP8Raster & reconstructed_raster = reconstructed_raster_handle.get();

  frame.decode( { }, references_, reconstructed_raster );
  references_.last = move( reconstructed_raster_handle );

  ivf_writer_.append_frame( frame.serialize( decoder_state_.probability_tables )  );
}
