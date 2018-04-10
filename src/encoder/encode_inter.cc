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
  enum { IS_SHORT, SIGN, SHORT, BITS = SHORT + 8 - 1, LONG_MV_WIDTH = 10 };

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

    for ( uint8_t i = 0; i < LONG_MV_WIDTH; i++ ) {
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

template<>
void Encoder::update_decoder_state( const InterFrame & frame )
{
  if ( frame.header().refresh_entropy_probs ) {
    decoder_state_.probability_tables.update( frame.header() );
  }

  if ( frame.header().mode_lf_adjustments.initialized() ) {
    if ( decoder_state_.filter_adjustments.initialized() ) {
      decoder_state_.filter_adjustments.get().update( frame.header() );
    } else {
      decoder_state_.filter_adjustments.initialize( frame.header() );
    }
  } else {
    decoder_state_.filter_adjustments.clear();
  }
}

Encoder::MVSearchResult Encoder::diamond_search( const VP8Raster::Macroblock & original_mb,
                                                 VP8Raster::Macroblock & temp_mb,
                                                 InterFrameMacroblock & frame_mb,
                                                 const VP8Raster & reference,
                                                 const SafeRaster & safe_reference,
                                                 MotionVector base_mv,
                                                 MotionVector origin,
                                                 size_t step_size,
                                                 const size_t y_ac_qi ) const
{
  size_t first_step = step_size / 2;

  auto reference_mb = reference.macroblock( original_mb.Y.column(),
                                            original_mb.Y.row() );

  TwoDSubRange<uint8_t, 16, 16> & prediction = temp_mb.Y.mutable_contents();

  base_mv = Scorer::clamp( base_mv, frame_mb.context() );

  constexpr array<array<int16_t, 2>, 5> check_sites = {{
    { -1, 0 }, { 0, -1 }, { 0, 0 }, { 0, 1 }, { 1, 0 }
  }};

  while ( step_size > 1 ) {
    MBPredictionData best_pred;
    MBPredictionData pred;

    for ( const auto & check_site : check_sites ) {
      pred.mv = MotionVector();
      MotionVector direction( step_size * check_site[ 0 ],
                              step_size * check_site[ 1 ] );

      pred.mv += origin + direction;

      if ( out_of_bounds( pred.mv ) ) continue;

      MotionVector this_mv( Scorer::clamp( pred.mv + base_mv, frame_mb.context() ) );

      reference_mb.Y().inter_predict( this_mv, safe_reference, prediction );
      pred.distortion = sad( original_mb.Y, prediction );
      pred.rate = costs_.sad_motion_vector_cost( pred.mv, MotionVector(), sad_per_bit16lut[ y_ac_qi ] );
      pred.cost = rdcost( pred.rate, pred.distortion, 1, 1 );

      if ( pred.cost < best_pred.cost  ) {
        best_pred = pred;
      }
    }

    if ( best_pred.mv == origin) {
      first_step = step_size / 2;
    }

    origin = best_pred.mv;
    step_size /= 2;
  }

  return { origin, first_step };
}

void Encoder::luma_mb_inter_predict( const VP8Raster::Macroblock & original_mb,
                                     VP8Raster::Macroblock & reconstructed_mb,
                                     VP8Raster::Macroblock & temp_mb,
                                     InterFrameMacroblock & frame_mb,
                                     const Quantizer & quantizer,
                                     MVComponentCounts & /* component_counts */,
                                     const size_t y_ac_qi,
                                     const EncoderPass encoder_pass )
{
  MBPredictionData best_pred;

  best_pred = luma_mb_best_prediction_mode( original_mb, reconstructed_mb, temp_mb,
                                            frame_mb, quantizer, encoder_pass, true );

  reference_frame frame_ref = LAST_FRAME;

  frame_mb.mutable_header().is_inter_mb = true;
  frame_mb.mutable_header().set_reference( frame_ref );

  MotionVector best_mv;
  const VP8Raster & reference = references_.at( frame_ref );
  const SafeRaster & safe_reference = safe_references_.get( frame_ref );

  const auto reference_mb = reference.macroblock( original_mb.Y.column(),
                                                  original_mb.Y.row() );

  TwoDSubRange<uint8_t, 16, 16> & prediction = temp_mb.Y.mutable_contents();

  const Scorer census = frame_mb.motion_vector_census();
  const MotionVector best_ref = Scorer::clamp( census.best(), frame_mb.context() );
  const auto counts = census.mode_contexts();
  const ProbabilityArray< num_mv_refs > mv_ref_probs = {{ mv_counts_to_probs.at( counts.at( 0 ) ).at( 0 ),
                                                          mv_counts_to_probs.at( counts.at( 1 ) ).at( 1 ),
                                                          mv_counts_to_probs.at( counts.at( 2 ) ).at( 2 ),
                                                          mv_counts_to_probs.at( counts.at( 3 ) ).at( 3 ) }};

  costs_.fill_mv_ref_costs( mv_ref_probs );

  constexpr array<mbmode, 4> inter_modes = { ZEROMV, NEARESTMV, NEARMV, NEWMV, /* SPLIMV */ };

  for ( const mbmode prediction_mode : inter_modes ) {
    MBPredictionData pred;
    pred.prediction_mode = prediction_mode;
    MotionVector mv;

    switch ( prediction_mode ) {
    case NEWMV:
      /* In the case of REALTIME_QUALITY, we should limit the number of times
       * that we search for a new motion vector.
       */
      if ( encode_quality_ == REALTIME_QUALITY ) {
        if ( not ( frame_mb.context().column % 4 == 0 and frame_mb.context().row % 4 == 0 ) ) {
          continue;
        }
      }

      for ( int step = 512; step > 1; ) {
        MVSearchResult result = diamond_search( original_mb, temp_mb, frame_mb,
                                                reference, safe_reference,
                                                best_ref, mv, step, y_ac_qi );

        if ( result.mv == mv ) {
          break; // there's no need to continue the search
        }

        mv = result.mv;
        step = result.first_step;
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

    reference_mb.macroblock().Y.inter_predict( mv, safe_reference, prediction );

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

  if ( best_pred.prediction_mode <= B_PRED ) {
    frame_mb.mutable_header().is_inter_mb = false;
    frame_mb.mutable_header().set_reference( CURRENT_FRAME );

    luma_mb_apply_intra_prediction( original_mb, reconstructed_mb, temp_mb,
                                    frame_mb, quantizer, best_pred.prediction_mode,
                                    encoder_pass );
  }
  else {
    frame_mb.mutable_header().is_inter_mb = true;
    frame_mb.mutable_header().set_reference( frame_ref );

    luma_mb_apply_inter_prediction( original_mb, reconstructed_mb, frame_mb,
                                    quantizer, best_pred.prediction_mode,
                                    best_mv );
  }
}

/*
 * Please refer to luma_mb_apply_intra_prediction for some information
 * about this method.
 */
void Encoder::luma_mb_apply_inter_prediction( const VP8Raster::Macroblock & original_mb,
                                              VP8Raster::Macroblock & reconstructed_mb,
                                              InterFrameMacroblock & frame_mb,
                                              const Quantizer & quantizer,
                                              const mbmode best_pred,
                                              const MotionVector best_mv )
{
  frame_mb.Y2().set_prediction_mode( best_pred );
  frame_mb.set_base_motion_vector( best_mv );

  if ( best_pred == SPLITMV ) {
    frame_mb.Y().forall_ij(
      [&] ( YBlock & frame_sb, unsigned int sb_column, unsigned int sb_row )
      {
        auto & original_sb = original_mb.Y_sub_at( sb_column, sb_row );

        frame_sb.mutable_coefficients().subtract_dct( original_sb,
          reconstructed_mb.Y_sub_at( sb_column, sb_row ).contents() );

        frame_sb.set_Y_without_Y2();
        frame_sb.mutable_coefficients() = YBlock::quantize( quantizer, frame_sb.coefficients() );
        frame_sb.calculate_has_nonzero();
      }
    );

    frame_mb.Y2().set_coded( false );
    frame_mb.Y2().calculate_has_nonzero();

    frame_mb.calculate_has_nonzero();
  }
  else {
    frame_mb.Y().forall(
      [&] ( YBlock & frame_sb ) { frame_sb.set_motion_vector( frame_mb.base_motion_vector() ); }
    );

    SafeArray<int16_t, 16> walsh_input;

    // XXX refactor this and its keyframe counterpart to remove the redundancy.
    frame_mb.Y().forall_ij(
      [&] ( YBlock & frame_sb, unsigned int sb_column, unsigned int sb_row )
      {
        auto & original_sb = original_mb.Y_sub_at( sb_column, sb_row );

        frame_sb.mutable_coefficients().subtract_dct( original_sb,
          reconstructed_mb.Y_sub_at( sb_column, sb_row ).contents() );

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
                                       VP8Raster::Macroblock & /* temp_mb */,
                                       InterFrameMacroblock & frame_mb,
                                       const Quantizer & quantizer,
                                       const EncoderPass ) const
{
  assert( frame_mb.inter_coded() );

  const VP8Raster & reference = references_.at( frame_mb.header().reference() );

  auto reference_mb = reference.macroblock( original_mb.Y.column(),
                                            original_mb.Y.row() );

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

  if ( frame_mb.y_prediction_mode() == SPLITMV ) {
    frame_mb.U().forall_ij(
      [&] ( UVBlock & block, const unsigned int column, const unsigned int row )
      {
        reference_mb.U_sub_at( column, row ).inter_predict( block.motion_vector(), reference.U(),
                                                            reconstructed_mb.U_sub_at( column, row ).mutable_contents() );
        reference_mb.V_sub_at( column, row ).inter_predict( block.motion_vector(), reference.V(),
                                                            reconstructed_mb.V_sub_at( column, row ).mutable_contents() );
      }
    );
  }
  else {
    reference_mb.U().inter_predict( frame_mb.U().at( 0, 0 ).motion_vector(),
                                  reference.U(), reconstructed_mb.U.mutable_contents() );
    reference_mb.V().inter_predict( frame_mb.U().at( 0, 0 ).motion_vector(),
                                  reference.V(), reconstructed_mb.V.mutable_contents() );
  }

  frame_mb.U().forall_ij(
    [&] ( UVBlock & frame_sb, unsigned int sb_column, unsigned int sb_row )
    {
      auto & original_sb = original_mb.U_sub_at( sb_column, sb_row );

      frame_sb.mutable_coefficients().subtract_dct( original_sb,
        reconstructed_mb.U_sub_at( sb_column, sb_row ).contents() );

      frame_sb.mutable_coefficients() = UVBlock::quantize( quantizer, frame_sb.coefficients() );
      frame_sb.calculate_has_nonzero();
    }
  );

  frame_mb.V().forall_ij(
    [&] ( UVBlock & frame_sb, unsigned int sb_column, unsigned int sb_row )
    {
      auto & original_sb = original_mb.V_sub_at( sb_column, sb_row );

      frame_sb.mutable_coefficients().subtract_dct( original_sb,
        reconstructed_mb.V_sub_at( sb_column, sb_row ).contents() );

      frame_sb.mutable_coefficients() = UVBlock::quantize( quantizer, frame_sb.coefficients() );
      frame_sb.calculate_has_nonzero();
    }
  );

  frame_mb.calculate_has_nonzero();
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
pair<InterFrame &, double> Encoder::encode_raster<InterFrame>( const VP8Raster & raster,
                                                               const QuantIndices & quant_indices,
                                                               const bool update_state,
                                                               const bool compute_ssim )
{
  DecoderState decoder_state_copy = decoder_state_;

  InterFrame & frame = inter_frame_;

  frame.mutable_header().quant_indices = quant_indices;
  frame.mutable_header().refresh_entropy_probs = true;
  frame.mutable_header().refresh_last = true;

  Quantizer quantizer( frame.header().quant_indices );
  MutableRasterHandle reconstructed_raster_handle { width(), height() };

  update_rd_multipliers( quantizer );

  costs_.fill_token_costs( ProbabilityTables() );

  TokenBranchCounts token_branch_counts;
  MVComponentCounts component_counts;

  costs_.fill_mv_component_costs( decoder_state_.probability_tables.motion_vector_probs );
  costs_.fill_mv_sad_costs();

  raster.macroblocks_forall_ij(
    [&] ( VP8Raster::ConstMacroblock original_mb, unsigned int mb_column, unsigned int mb_row )
    {
      auto reconstructed_mb = reconstructed_raster_handle.get().macroblock( mb_column, mb_row );
      auto temp_mb = temp_raster().macroblock( mb_column, mb_row );
      auto & frame_mb = frame.mutable_macroblocks().at( mb_column, mb_row );

      // Process Y and Y2
      luma_mb_inter_predict( original_mb.macroblock(), reconstructed_mb, temp_mb, frame_mb,
                             quantizer, component_counts,
                             frame.header().quant_indices.y_ac_qi, FIRST_PASS );

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
  apply_best_loopfilter_settings( raster, reconstructed_raster_handle.get(), frame );

  RasterHandle immutable_raster( move( reconstructed_raster_handle ) );

  if ( not update_state ) {
    decoder_state_ = decoder_state_copy;
  }

  return { frame,
           compute_ssim ? immutable_raster.get().quality( raster ) : 0.0 };
}
