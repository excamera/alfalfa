/*
 *  Copyright (c) 2013 Anirudh Sivaraman
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/* Maintain a frame's dependencies (both implicit and explicit)
 */

#include "frame_state.hh"

void FrameState::pretty_print_everything(void) const {
  pretty_print_raster_numbers();
  printf("Decoding headers of current frame %u\n", raster_num);
  pretty_print_frame_hdr();
  pretty_print_segment_hdr();
  pretty_print_loopfilter_hdr();
  pretty_print_token_hdr();
  pretty_print_quant_hdr();
  pretty_print_reference_hdr();
  pretty_print_frame_deps();
  pretty_print_entropy_hdr();
}

void FrameState::pretty_print_inter_frame_state(void) const {
  printf("Decoded headers of current frame %u\n", raster_num);
  pretty_print_raster_numbers();
  pretty_print_frame_hdr();
  pretty_print_reference_hdr();
  pretty_print_frame_deps();
  pretty_print_entropy_hdr();
}

void FrameState::pretty_print_frame_hdr(void) const {
  printf("********** Uncompressed data chunk (Sec 9.1) ********* \n");
  printf("Is it a keyframe      ? %u\n", frame_hdr.is_keyframe);
  printf("Is it experimental    ? %u\n", frame_hdr.is_experimental);
  printf("Version number (pg 31): %u\n", frame_hdr.version);
  printf("Is frame visible      ? %u\n", frame_hdr.is_shown);
  printf("Partition1 size (pg 8): %u\n", frame_hdr.part0_sz);
  printf("Frame width           : %u\n", frame_hdr.kf.w);
  printf("Frame height          : %u\n", frame_hdr.kf.h);
  printf("Width scaling  (pg 33): %u\n", frame_hdr.kf.scale_w);
  printf("Height scaling (pg 33): %u\n", frame_hdr.kf.scale_h);
  printf("Resolution updated    ? %u\n", frame_hdr.frame_size_updated);
  printf("\n\n");
}

void FrameState::pretty_print_segment_hdr(void) const {
  printf("********** Segment based adjustments (Sec 10, 9.3) ******** \n");
  printf("segmentation enabled  ? %u\n", segment_hdr.enabled);
  if (segment_hdr.enabled) {
    /* I think the probabilities alone will suffice */
//    printf("update_segment_feature_data (pg 125)         ? %u\n", segment_hdr.update_data);
//    printf("update_segment_map (pg 125)                  ? %u\n", segment_hdr.update_map);
//    printf("Absolute/delta updates (pg 125)              ? %u\n", segment_hdr.abs);
    printf("Probabilities for each segment id  :");
    for (uint32_t i=0; i < MB_FEATURE_TREE_PROBS; i++) {
      printf("%u %u,", i, segment_hdr.tree_probs[i]);
    }
    printf("\n");
    printf("Loop filter for each segment id    :");
    for (uint32_t i = 0; i< MAX_MB_SEGMENTS; i++) {
      printf("%u %d,", i, segment_hdr.lf_level[i]);
    }
    printf("\n");
    printf("Quantizer level for each segment id:");
    for (uint32_t i = 0; i< MAX_MB_SEGMENTS; i++) {
      printf("%u %d,", i, segment_hdr.quant_idx[i]);
    }
    printf("\n");
  }
  printf("\n\n");
}

void FrameState::pretty_print_loopfilter_hdr(void) const {
  printf("************ Loopfilter levels (Sec 9.4) ********* \n");
  printf("Use simple loop filter (pg 35)      ? %u\n", loopfilter_hdr.use_simple);
  printf("Loop filter Level      (pg 35)      : %u\n", loopfilter_hdr.level);
  printf("sharpness              (pg 35)      : %u\n", loopfilter_hdr.sharpness);
  printf("Per macroblock loop filter adjustment enabled (pg 36) ? %u\n", loopfilter_hdr.delta_enabled);
  if (loopfilter_hdr.delta_enabled) {
    printf("Loop filter deltas based on reference frame (pg 36) :\n");
    for (uint32_t i = 0; i < BLOCK_CONTEXTS; i++) {
      printf("%u %d,", i, loopfilter_hdr.ref_delta[i]);
    }
    printf("\n");
    printf("Loop filter deltas based on prediction mode (pg 36) :\n");
    for (uint32_t i = 0; i < BLOCK_CONTEXTS; i++) {
      printf("%u %d,", i, loopfilter_hdr.mode_delta[i]);
    }
    printf("\n");
  }
  printf("\n\n");
}

void FrameState::pretty_print_token_hdr(void) const {
  printf("*********** Token header (Sec 9.5) ********** \n");
  printf("Number of paritions : %u\n", token_hdr.partitions);
  printf("Partition sizes     : \n");
  for (uint32_t i = 0; i < MAX_PARTITIONS; i++) {
    printf("%u %u,", i, token_hdr.partition_sz[i]);
  }
  printf("\n");
  printf("\n\n");
}

void FrameState::pretty_print_quant_hdr(void) const {
  printf("************ Dequantization indices (Sec 9.6) *********** \n");
  printf("    q_index : %u\n", quant_hdr.q_index);
  printf("delta_update (I think this is a bit packed version of all 5 deltas below): %d\n", quant_hdr.delta_update);
  printf("Deltas relative to dequantization coefficient indexed by q_index : \n");
  printf("Y1 DC delta : %d\n", quant_hdr.y1_dc_delta_q);
  printf("Y2 DC delta : %d\n", quant_hdr.y2_dc_delta_q);
  printf("Y2 AC delta : %d\n", quant_hdr.y2_ac_delta_q);
  printf("UV DC delta : %d\n", quant_hdr.uv_dc_delta_q);
  printf("UV AC delta : %d\n", quant_hdr.uv_ac_delta_q);
  printf("\n\n");
}

void FrameState::pretty_print_reference_hdr(void) const {
  printf("************* Refresh Golden and altref frame (Sec 9.7) *********** \n");
  printf("refresh golden frame                ? %u\n", reference_hdr.refresh_gf);
  printf("refresh altref frame                ? %u\n", reference_hdr.refresh_arf);
  printf("buffer to copy into golden frame    ? %u\n", reference_hdr.copy_gf);
  printf("buffer to copy into altref frame    ? %u\n", reference_hdr.copy_arf);
  printf("reference hdr sign bias (pg 39) to control sign of motion vectors \n");
  for (uint32_t i = 0; i < 4; i++) {
    printf("%u %u,", i, reference_hdr.sign_bias[i]);
  }
  printf("\n");
  printf("Persist changes in entropy state (pg 124)? %u\n", reference_hdr.refresh_entropy);
  printf("refresh last frame buffer (Sec 9.8)      ? %u\n", reference_hdr.refresh_last);
  printf("\n\n");
}

void FrameState::pretty_print_raster_numbers(void) const {
  printf("************** IDs of different rasters *************************\n");
  printf("Last frame   : %u \n", raster_ids.lf_number);
  printf("Golden frame : %u \n", raster_ids.gf_number);
  printf("Altref frame : %u \n", raster_ids.ar_number);
  printf("\n\n");
}

void FrameState::pretty_print_frame_deps(void) const {
  /* Print out union of all macroblock dependencies for this frame */
  printf("************** Frame dependency *************************\n");
//  printf("Last frame   ? %u \n", reference_hdr.depends_lf);
//  printf("Golden frame ? %u \n", reference_hdr.depends_gf);
//  printf("Altref frame ? %u \n", reference_hdr.depends_ar);
  printf("\n\n");
}

void FrameState::pretty_print_entropy_hdr(void) const {
  printf("************ Entropy header (Sec 9.9, 9.10, 9.11)  *********** \n");
  printf("DCT coefficient probability table (Sec 9.9,13)    : \n");
  for (uint32_t i = 0; i < BLOCK_TYPES; i++)
    for (uint32_t j = 0; j < COEF_BANDS; j++)
      for (uint32_t k = 0; k < PREV_COEF_CONTEXTS; k++)
        for (uint32_t l = 0; l < ENTROPY_NODES; l++)
          printf(" %u %u %u %u %u \n", i, j, k, l,
                 entropy_hdr.coeff_probs[i][j][k][l]);

  printf("Motion vector probability table  (Sec 17.2)     : \n");
  for (uint32_t i = 0; i < 2; i++)
    for (uint32_t j = 0; j < MV_PROB_CNT; j++)
      printf("%u %u %u \n", i, j, entropy_hdr.mv_probs[i][j]);

  printf("Skip all-zero macroblocks (First 2 fields in Sec 9.10,9.11) ? %u\n", entropy_hdr.coeff_skip_enabled);
  printf("Probability that a macroblock has no non-zero coefficients (Sec 9.11) %u\n", entropy_hdr.coeff_skip_prob);
  printf("Y mode motion vector prediction mode probabilities (Sec 9.10) : \n");
  for (uint32_t i = 0; i < 4; i++) {
    printf(" %u %u,", i, entropy_hdr.y_mode_probs[i]);
  }
  printf("\n");
  printf("UV mode motion vector prediction mode probabilities (Sec 9.10): \n");
  for (uint32_t i = 0; i < 3; i++) {
    printf(" %u %u,", i, entropy_hdr.uv_mode_probs[i]);
  }
  printf("\n");
  printf("Probability that current frame is an inter_frame (pg 40)            : %u\n", entropy_hdr.prob_inter);
  printf("Probability that inter_frame is predicted from last_frame (pg 40)   : %u\n", entropy_hdr.prob_last);
  printf("Probability that inter_frame is predicted from golden_frame (pg 40) : %u\n", entropy_hdr.prob_gf);
  printf("\n\n");
}
