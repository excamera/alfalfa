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
#include <cmath>

#include "encoder.hh"
#include "scorer.hh"

using namespace std;

template<class FrameType>
InterFrame Encoder::reencode_as_interframe( const VP8Raster & original_raster,
                                            const FrameType & original_frame,
                                            const QuantIndices & quant_indices )
{
  InterFrame frame { width(), height() };

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
  if_header.quant_indices           = quant_indices;
  if_header.refresh_last            = true;
  if_header.refresh_golden_frame    = true;
  if_header.refresh_alternate_frame = true;
  if_header.refresh_entropy_probs   = true;
  if_header.copy_buffer_to_golden.clear();
  if_header.copy_buffer_to_alternate.clear();

  if_header.intra_16x16_prob.reset();
  if_header.intra_chroma_prob.reset();

  for ( size_t i = 0; i < k_default_y_mode_probs.size(); i++ ) {
    if_header.intra_16x16_prob.get().at( i ) = k_default_y_mode_probs.at( i );
  }

  for ( size_t i = 0; i < k_default_uv_mode_probs.size(); i++ ) {
    if_header.intra_chroma_prob.get().at( i ) = k_default_uv_mode_probs.at( i );
  }

  Quantizer quantizer( frame.header().quant_indices );
  MutableRasterHandle reconstructed_raster_handle { width(), height() };
  VP8Raster & reconstructed_raster = reconstructed_raster_handle.get();

  MVComponentCounts component_counts;
  TokenBranchCounts token_branch_counts;

  ProbabilityTables temp_tables = decoder_state_.probability_tables;
  temp_tables.update( if_header );
  costs_.fill_mv_component_costs( temp_tables.motion_vector_probs );

  original_raster.macroblocks_forall_ij(
    [&] ( VP8Raster::ConstMacroblock original_mb, unsigned int mb_column, unsigned int mb_row )
    {
      auto reconstructed_mb = reconstructed_raster.macroblock( mb_column, mb_row );
      auto temp_mb = temp_raster().macroblock( mb_column, mb_row );
      auto & frame_mb = frame.mutable_macroblocks().at( mb_column, mb_row );

      // Process Y and Y2
      luma_mb_inter_predict( original_mb.macroblock(), reconstructed_mb, temp_mb,
                             frame_mb, quantizer, component_counts,
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

  apply_best_loopfilter_settings( original_raster, reconstructed_raster, frame );

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
                                     const InterFrame & original_frame,
                                     const QuantIndices & quant_indices,
                                     const bool last_frame )
{
  InterFrame frame { width(), height() };

  const InterFrameHeader & of_header = original_frame.header();
  InterFrameHeader & if_header = frame.mutable_header();

  if_header.update_segmentation      = of_header.update_segmentation;
  if_header.filter_type              = of_header.filter_type;
  if_header.loop_filter_level        = of_header.loop_filter_level;
  if_header.sharpness_level          = of_header.sharpness_level;
  if_header.mode_lf_adjustments      = of_header.mode_lf_adjustments;
  if_header.sign_bias_golden         = of_header.sign_bias_golden;
  if_header.sign_bias_alternate      = of_header.sign_bias_alternate;
  if_header.refresh_entropy_probs    = of_header.refresh_entropy_probs;
  if_header.prob_references_last     = of_header.prob_references_last;
  if_header.prob_references_golden   = of_header.prob_references_golden;

  if ( last_frame ) {
    if_header.refresh_last            = true;
    if_header.refresh_golden_frame    = true;
    if_header.refresh_alternate_frame = true;
    if_header.copy_buffer_to_golden.clear();
    if_header.copy_buffer_to_alternate.clear();
  }
  else {
    if_header.refresh_last             = of_header.refresh_last;
    if_header.refresh_golden_frame     = of_header.refresh_golden_frame;
    if_header.refresh_alternate_frame  = of_header.refresh_alternate_frame;
    if_header.copy_buffer_to_golden    = of_header.copy_buffer_to_golden;
    if_header.copy_buffer_to_alternate = of_header.copy_buffer_to_alternate;
  }

  if_header.quant_indices = quant_indices;

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

  return frame;
}

void Encoder::reencode( const vector<RasterHandle> & original_rasters,
                        const vector<pair<Optional<KeyFrame>, Optional<InterFrame>>> & prediction_frames,
                        const double kf_q_weight,
                        const bool extra_frame_chunk,
                        IVFWriter & ivf_writer )
{
  if ( original_rasters.empty() ) {
    throw runtime_error( "no rasters to re-encode" );
  } else if ( original_rasters.size() != prediction_frames.size() ) {
    throw runtime_error( "prediction/original_rasters mismatch" );
  }

  unsigned int start_frame_index = ( extra_frame_chunk ? 1 : 0 );

  for ( unsigned int frame_index = start_frame_index;
        frame_index < original_rasters.size();
        frame_index++ ) {
    const VP8Raster & target_output = original_rasters.at( frame_index ).get();

    bool last_frame = ( frame_index == prediction_frames.size() - 1 );

    if ( target_output.display_width() != width()
         or target_output.display_height() != height()
         or width() != ivf_writer.width()
         or height() != ivf_writer.height() ) {
      throw runtime_error( "raster size mismatch" );
    }

    const auto & prediction_frame_ref = prediction_frames.at( frame_index );

    /* Option 1: Is this an initial KeyFrame that should be re-encoded as an InterFrame? */
    if ( (frame_index == start_frame_index) and prediction_frame_ref.first.initialized() ) {
      QuantIndices new_quantizer = prediction_frame_ref.first.get().header().quant_indices;

      /* try to steal the quantizer from the next frame (if it's an interframe */
      if ( frame_index + 1 < prediction_frames.size() ) {
        const auto & next_frame_ref = prediction_frames.at( frame_index + 1 );
        if ( next_frame_ref.second.initialized() ) {
          /* it's an InterFrame */

          new_quantizer.y_ac_qi = lrint( kf_q_weight * prediction_frame_ref.first.get().header().quant_indices.y_ac_qi
                                         + ( 1 - kf_q_weight ) * next_frame_ref.second.get().header().quant_indices.y_ac_qi );
        }
      }

      ivf_writer.append_frame( write_frame( reencode_as_interframe( target_output, prediction_frame_ref.first.get(), new_quantizer ) ) );

      continue;
    } else if ( frame_index == start_frame_index and extra_frame_chunk ) {
      /* Option 2: Is this the first interframe of an extra-frame chunk?
         Then update the quantizer. */

      if ( not prediction_frames.at( 0 ).first.initialized() ) {
        throw runtime_error( "extra-frame chunks must start with a keyframe." );
      }

      QuantIndices new_quantizer = prediction_frame_ref.second.get().header().quant_indices;
      new_quantizer.y_ac_qi = lrint( kf_q_weight *  prediction_frames.at( 0 ).first.get().header().quant_indices.y_ac_qi
                                     + ( 1 - kf_q_weight ) * prediction_frame_ref.second.get().header().quant_indices.y_ac_qi );

      ivf_writer.append_frame( write_frame( update_residues( target_output,
                                                             prediction_frame_ref.second.get(),
                                                             new_quantizer, last_frame ) ) );
    } else if ( prediction_frame_ref.first.initialized() ) {
      /* Option 3: Is this another KeyFrame? Then preserve it. */
      ivf_writer.append_frame( write_frame( prediction_frame_ref.first.get() ) );
    } else if ( prediction_frame_ref.second.initialized() ) {
      /* Option 4: Is this an InterFrame? Then update residues. */
      ivf_writer.append_frame( write_frame( update_residues( target_output,
                                                             prediction_frame_ref.second.get(),
                                                             prediction_frame_ref.second.get().header().quant_indices,
                                                             last_frame ) ) );
    } else {
      throw runtime_error( "prediction_frames contained two undefined values" );
    }
  }
}
