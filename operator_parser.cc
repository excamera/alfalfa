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

#include "./dixie.h"
#include "./vp8_prob_data.h"
#include "./operator_parser.hh"

OperatorParser::OperatorParser(struct vp8_decoder_ctx* t_ctx,
                               const unsigned char*    t_data,
                               unsigned int            t_size)
    : ctx(t_ctx),
      data(t_data),
      sz(t_size),
      raster_buffer_ids_({0, 0, 0, 0}) {}

void OperatorParser::decode_operator_headers(void) {
  vpx_codec_err_t  res;
  struct bool_decoder  entropy_decoder;
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

  decode_entropy_header(ctx, &entropy_decoder, &ctx->entropy_hdr);
  ctx->frame_cnt++;

  if (!ctx->reference_hdr.refresh_entropy) {
      ctx->entropy_hdr = ctx->saved_entropy;
      ctx->saved_entropy_valid = 0;
  }
}

void OperatorParser::update_ref_rasters(void) {
  /* Set it equal to the current vp8_raster_buffer_ids state */
  struct vp8_raster_buffer_ids new_raster_buffers = raster_buffer_ids_;
  const struct vp8_raster_buffer_ids current = raster_buffer_ids_;

  /* forcibly update the current_raster_num */
  new_raster_buffers.raster_num = ctx->frame_cnt;

  /* Handle reference raster updates */
  if (ctx->reference_hdr.copy_arf == 1) {
      new_raster_buffers.ar_number = current.lf_number;
  } else if (ctx->reference_hdr.copy_arf == 2) {
      new_raster_buffers.ar_number = current.gf_number;
  }

  if (ctx->reference_hdr.copy_gf == 1) {
      new_raster_buffers.gf_number = current.lf_number;
  } else if (ctx->reference_hdr.copy_gf == 2) {
      new_raster_buffers.gf_number = current.ar_number;
  }

  if (ctx->reference_hdr.refresh_gf) {
      new_raster_buffers.gf_number = ctx->frame_cnt;
  }

  if (ctx->reference_hdr.refresh_arf) {
      new_raster_buffers.ar_number = ctx->frame_cnt;
  }

  if (ctx->reference_hdr.refresh_last) {
      new_raster_buffers.lf_number = ctx->frame_cnt;
  }

  raster_buffer_ids_ = new_raster_buffers;
}

FrameState OperatorParser::get_frame_state(void) {
  FrameState ret(ctx->frame_hdr,
                 ctx->segment_hdr,
                 ctx->loopfilter_hdr,
                 ctx->token_hdr,
                 ctx->quant_hdr,
                 ctx->reference_hdr,
                 ctx->entropy_hdr,
                 raster_buffer_ids_);
  return ret;
}
