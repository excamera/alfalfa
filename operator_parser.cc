/*
 *  Copyright (c) 2013 Anirudh Sivaraman
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/* Parse header of each Operator to update FrameState
 *
 */

#include <assert.h>
#include <string.h>

#include "dixie.hh"
#include "vp8_prob_data.hh"
#include "operator_parser.hh"
#include "modemv_data.hh"

OperatorParser::OperatorParser(struct vp8_decoder_ctx*   t_ctx,
                               const unsigned char*      t_data,
                               unsigned int              t_size,
                               struct vp8_raster_ref_ids t_raster_ref_ids)
    : ctx(t_ctx),
      data(t_data),
      sz(t_size),
      raster_ref_ids_(t_raster_ref_ids),
      raster_num(0),
      entropy_decoder({nullptr, 0, 0, 0, 0}) {}


void OperatorParser::decode_operator_headers(void) {
  vpx_codec_err_t  res;
  int                  i, row, partition;

  ctx->saved_entropy_valid = 0;

  if ((res = vp8_parse_frame_header(data, sz, &ctx->frame_hdr)))
      vpx_internal_error(&ctx->error, res,
                         "Failed to parse frame header");

  if (ctx->frame_hdr.is_experimental)
      vpx_internal_error(&ctx->error, VPX_CODEC_UNSUP_BITSTREAM,
                         "Experimental bitstreams not supported.");

  data += FRAME_HEADER_SZ;
  sz -= FRAME_HEADER_SZ;

  if (ctx->frame_hdr.is_keyframe) {
      data += KEYFRAME_HEADER_SZ;
      sz -= KEYFRAME_HEADER_SZ;
      ctx->mb_cols = (ctx->frame_hdr.kf.w + 15) / 16;
      ctx->mb_rows = (ctx->frame_hdr.kf.h + 15) / 16;
  }

  /* Start the bitreader for the header/entropy partition */
  init_bool_decoder(&entropy_decoder, data, ctx->frame_hdr.part0_sz);

  /* Skip the colorspace and clamping bits */
  if (ctx->frame_hdr.is_keyframe)
      if (bool_get_uint(&entropy_decoder, 2))
          vpx_internal_error(&ctx->error, VPX_CODEC_UNSUP_BITSTREAM,
                             "Reserved bits not supported.");

  decode_segmentation_header(ctx, &entropy_decoder, &ctx->segment_hdr);
  decode_loopfilter_header(ctx, &entropy_decoder, &ctx->loopfilter_hdr);
  decode_and_init_token_partitions(ctx,
                                   &entropy_decoder,
                                   data + ctx->frame_hdr.part0_sz,
                                   sz - ctx->frame_hdr.part0_sz,
                                   &ctx->token_hdr);
  decode_quantizer_header(ctx, &entropy_decoder, &ctx->quant_hdr);
  decode_reference_header(ctx, &entropy_decoder, &ctx->reference_hdr);

  /* Set keyframe entropy defaults. These get updated on keyframes
   * regardless of the refresh_entropy setting.
   */
  if (ctx->frame_hdr.is_keyframe) {
      ARRAY_COPY(ctx->entropy_hdr.coeff_probs,
                 k_default_coeff_probs);
      ARRAY_COPY(ctx->entropy_hdr.mv_probs,
                 k_default_mv_probs);
      ARRAY_COPY(ctx->entropy_hdr.y_mode_probs,
                 k_default_y_mode_probs);
      ARRAY_COPY(ctx->entropy_hdr.uv_mode_probs,
                 k_default_uv_mode_probs);
  }

  if (!ctx->reference_hdr.refresh_entropy) {
      ctx->saved_entropy = ctx->entropy_hdr;
      ctx->saved_entropy_valid = 1;
  }

  int current_len = entropy_decoder.input_len;
  decode_entropy_header(ctx, &entropy_decoder, &ctx->entropy_hdr);
  prob_update_len_ = current_len - entropy_decoder.input_len;

  ctx->frame_cnt++;

  if (!ctx->reference_hdr.refresh_entropy) {
      ctx->entropy_hdr = ctx->saved_entropy;
      ctx->saved_entropy_valid = 0;
  }

  /* forcibly update the current raster num */
  raster_num = ctx->frame_cnt;
  assert(raster_num > 0);
}

struct mv_clamp_rect OperatorParser::get_initial_bounds(int row) {
  /* Calculate the initial eighth-pel MV bounds
     for this row using a 1 MB border. */
  struct mv_clamp_rect  bounds;
  bounds.to_left   = -(1 << 7);
  bounds.to_right  = (ctx->mb_cols) << 7;
  bounds.to_top    = -((row + 1) << 7);
  bounds.to_bottom = (ctx->mb_rows - row) << 7;
  return bounds;
}

/* Return golden, altref, and last frame dependencies for current mb */
struct vp8_mb_dependencies OperatorParser::mb_dependencies(const struct mb_info *current) {
  struct vp8_mb_dependencies deps = {false, false, false};
  if (current->base.ref_frame == LAST_FRAME) {
    deps.depends_lf = true;
  } else if (current->base.ref_frame == GOLDEN_FRAME) {
    deps.depends_gf = true;
  } else if (current->base.ref_frame == ALTREF_FRAME) {
    deps.depends_ar = true;
  } else {
    assert(false);
  }
  return deps;
}

/*
  Page 9 of the spec says macroblock prediction data
  can be decoded independent of residual data.
  This function decodes macroblock prediction records
  alone to discover raster dependencies
 */

struct vp8_mb_dependencies OperatorParser::decode_macroblock_data(void) {
  /*
     Allocate space to store macroblock records as
     2D arrays (raster order) within ctx
     This structure is freed up and reallocated on every
     resolution update (on key frames).
   */
  vp8_dixie_modemv_init(ctx);

  /*
     If it's a keyframe, there are no dependencies, so
     simply set all depends_* to 0 and return
   */
  if (ctx->frame_hdr.is_keyframe) {
    return (raster_deps_ = {false, false, false});
  }

  /* Ensure it's not a keyframe */
  assert(!ctx->frame_hdr.is_keyframe);

  /* Track current mb, left mb and above mb */
  struct mb_info *current, *left, *above;

  /* Maintain row and column index */
  int row = 0;
  int col = 0;

  /* Run through macroblocks in raster scan order (Page 10) */
  struct vp8_mb_dependencies deps_union = {false, false, false};
  for (row = 0; row < ctx->mb_rows; row++) {
    /* Calculate the initial eighth-pel MV bounds
       for this row using a 1 MB border. */
    struct mv_clamp_rect  bounds = get_initial_bounds(row);

    /* Get pointer to first macroblock prediction record in this row */
    current = ctx->mb_info_rows[row] + 0;
    above   = ctx->mb_info_rows[row - 1] + 0;
    left    = current - 1;

    /* Now run through all columns */
    for (col = 0; col < ctx->mb_cols; col++) {
      /* Parse according to syntax in Page 130 */
      if (ctx->segment_hdr.update_map) {
        current->base.segment_id = read_segment_id(&entropy_decoder,
                                                   &ctx->segment_hdr);
      }

      if (ctx->entropy_hdr.coeff_skip_enabled) {
        current->base.skip_coeff = bool_get(&entropy_decoder,
                                            ctx->entropy_hdr.coeff_skip_prob);
      }

      if (bool_get(&entropy_decoder, ctx->entropy_hdr.prob_inter)) {
        /* It's an inter macroblock => Get reference frame */
        decode_mvs(ctx, current, left, above, &bounds, &entropy_decoder);

        /* Accumulate union of all mb deps */
        deps_union = mb_dependencies(current).compute_union(deps_union);
      } else {
        /* It's an intra macroblock, simply consume it, as usual */
        decode_intra_mb_mode(current, &ctx->entropy_hdr, &entropy_decoder);
      }

      /* Correct bounds for the next set */
      bounds.to_left -= 16 << 3;
      bounds.to_right -= 16 << 3;

      /* Advance to next mb */
      current++;
      above++;
      left++;
    }
  }
  return (raster_deps_ = deps_union);
}

struct vp8_raster_ref_ids OperatorParser::update_ref_rasters(void) {
  /* Set it equal to the current vp8_raster_ref_ids state */
  struct vp8_raster_ref_ids new_raster_refs = raster_ref_ids_;
  const struct vp8_raster_ref_ids current = raster_ref_ids_;

  /* Handle reference raster updates */
  if (ctx->reference_hdr.copy_arf == 1) {
      new_raster_refs.ar_number = current.lf_number;
  } else if (ctx->reference_hdr.copy_arf == 2) {
      new_raster_refs.ar_number = current.gf_number;
  }

  if (ctx->reference_hdr.copy_gf == 1) {
      new_raster_refs.gf_number = current.lf_number;
  } else if (ctx->reference_hdr.copy_gf == 2) {
      new_raster_refs.gf_number = current.ar_number;
  }

  if (ctx->reference_hdr.refresh_gf) {
      new_raster_refs.gf_number = ctx->frame_cnt;
  }

  if (ctx->reference_hdr.refresh_arf) {
      new_raster_refs.ar_number = ctx->frame_cnt;
  }

  if (ctx->reference_hdr.refresh_last) {
      new_raster_refs.lf_number = ctx->frame_cnt;
  }

  raster_ref_ids_ = new_raster_refs;
  return raster_ref_ids_;
}

FrameState OperatorParser::get_frame_state(void) {
  FrameState ret(ctx->frame_hdr,
                 ctx->segment_hdr,
                 ctx->loopfilter_hdr,
                 ctx->token_hdr,
                 ctx->quant_hdr,
                 ctx->reference_hdr,
                 ctx->entropy_hdr,
                 raster_ref_ids_,
                 raster_deps_,
                 raster_num,
                 prob_update_len_);
  return ret;
}
