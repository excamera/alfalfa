/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "./vpx_codec_internal.h"
#include "bit_ops.h"
#include "dixie.h"
#include "vp8_prob_data.h"
#include "dequant_data.h"
#include "modemv.h"
#include "tokens.h"
#include "predict.h"
#include "dixie_loopfilter.h"
#include <string.h>
#include <assert.h>

void
decode_entropy_header(struct vp8_decoder_ctx    *ctx,
                      struct bool_decoder       *boolean_decoder,
                      struct vp8_entropy_hdr    *hdr)
{
    int i, j, k, l;

    /* Read coefficient probability updates */
    for (i = 0; i < BLOCK_TYPES; i++)
        for (j = 0; j < COEF_BANDS; j++)
            for (k = 0; k < PREV_COEF_CONTEXTS; k++)
                for (l = 0; l < ENTROPY_NODES; l++)
                    if (bool_get(boolean_decoder,
                                 k_coeff_entropy_update_probs
                                     [i][j][k][l]))
                        hdr->coeff_probs[i][j][k][l] =
                            bool_get_uint(boolean_decoder, 8);

    /* Read coefficient skip mode probability */
    hdr->coeff_skip_enabled = bool_get_bit(boolean_decoder);

    if (hdr->coeff_skip_enabled)
        hdr->coeff_skip_prob = bool_get_uint(boolean_decoder, 8);

    /* Parse interframe probability updates */
    if (!ctx->frame_hdr.is_keyframe)
    {
        hdr->prob_inter = bool_get_uint(boolean_decoder, 8);
        hdr->prob_last  = bool_get_uint(boolean_decoder, 8);
        hdr->prob_gf    = bool_get_uint(boolean_decoder, 8);

        if (bool_get_bit(boolean_decoder))
            for (i = 0; i < 4; i++)
                hdr->y_mode_probs[i] = bool_get_uint(boolean_decoder, 8);

        if (bool_get_bit(boolean_decoder))
            for (i = 0; i < 3; i++)
                hdr->uv_mode_probs[i] = bool_get_uint(boolean_decoder, 8);

        for (i = 0; i < 2; i++)
            for (j = 0; j < MV_PROB_CNT; j++)
                if (bool_get(boolean_decoder, k_mv_entropy_update_probs[i][j]))
                {
                    int x = bool_get_uint(boolean_decoder, 7);
                    hdr->mv_probs[i][j] = x ? x << 1 : 1;
                }
    }
}


void
decode_reference_header(struct vp8_decoder_ctx    *ctx,
                        struct bool_decoder       *boolean_decoder,
                        struct vp8_reference_hdr  *hdr)
{
    unsigned int key = ctx->frame_hdr.is_keyframe;

    hdr->refresh_gf    = key ? 1 : bool_get_bit(boolean_decoder);
    hdr->refresh_arf   = key ? 1 : bool_get_bit(boolean_decoder);
    hdr->copy_gf       = key ? 0 : !hdr->refresh_gf
                         ? bool_get_uint(boolean_decoder, 2) : 0;
    hdr->copy_arf      = key ? 0 : !hdr->refresh_arf
                         ? bool_get_uint(boolean_decoder, 2) : 0;
    hdr->sign_bias[GOLDEN_FRAME] = key ? 0 : bool_get_bit(boolean_decoder);
    hdr->sign_bias[ALTREF_FRAME] = key ? 0 : bool_get_bit(boolean_decoder);
    hdr->refresh_entropy = bool_get_bit(boolean_decoder);
    hdr->refresh_last  = key ? 1 : bool_get_bit(boolean_decoder);
}


void
decode_quantizer_header(struct vp8_decoder_ctx    *ctx,
                        struct bool_decoder       *boolean_decoder,
                        struct vp8_quant_hdr      *hdr)
{
    int update;
    int last_q = hdr->q_index;

    hdr->q_index = bool_get_uint(boolean_decoder, 7);
    update = last_q != hdr->q_index;
    update |= (hdr->y1_dc_delta_q = bool_maybe_get_int(boolean_decoder, 4));
    update |= (hdr->y2_dc_delta_q = bool_maybe_get_int(boolean_decoder, 4));
    update |= (hdr->y2_ac_delta_q = bool_maybe_get_int(boolean_decoder, 4));
    update |= (hdr->uv_dc_delta_q = bool_maybe_get_int(boolean_decoder, 4));
    update |= (hdr->uv_ac_delta_q = bool_maybe_get_int(boolean_decoder, 4));
    hdr->delta_update = update;
}


void
decode_and_init_token_partitions(struct vp8_decoder_ctx    *ctx,
                                 struct bool_decoder       *boolean_decoder,
                                 const unsigned char       *data,
                                 unsigned int               sz,
                                 struct vp8_token_hdr      *hdr)
{
    int i;

    hdr->partitions = 1 << bool_get_uint(boolean_decoder, 2);

    if (sz < 3 *(hdr->partitions - 1))
        vpx_internal_error(&ctx->error, VPX_CODEC_CORRUPT_FRAME,
                           "Truncated packet found parsing partition"
                           " lengths.");

    sz -= 3 * (hdr->partitions - 1);

    for (i = 0; i < hdr->partitions; i++)
    {
        if (i < hdr->partitions - 1)
        {
            hdr->partition_sz[i] = (data[2] << 16)
                                   | (data[1] << 8) | data[0];
            data += 3;
        }
        else
            hdr->partition_sz[i] = sz;

        if (sz < hdr->partition_sz[i])
            vpx_internal_error(&ctx->error, VPX_CODEC_CORRUPT_FRAME,
                               "Truncated partition %d", i);

        sz -= hdr->partition_sz[i];
    }


    for (i = 0; i < ctx->token_hdr.partitions; i++)
    {
        init_bool_decoder(&ctx->tokens[i].boolean_dec, data,
                          ctx->token_hdr.partition_sz[i]);
        data += ctx->token_hdr.partition_sz[i];
    }
}


void
decode_loopfilter_header(struct vp8_decoder_ctx    *ctx,
                         struct bool_decoder       *boolean_decoder,
                         struct vp8_loopfilter_hdr *hdr)
{
    if (ctx->frame_hdr.is_keyframe)
        memset(hdr, 0, sizeof(*hdr));

    hdr->use_simple    = bool_get_bit(boolean_decoder);
    hdr->level         = bool_get_uint(boolean_decoder, 6);
    hdr->sharpness     = bool_get_uint(boolean_decoder, 3);
    hdr->delta_enabled = bool_get_bit(boolean_decoder);

    if (hdr->delta_enabled && bool_get_bit(boolean_decoder))
    {
        int i;

        for (i = 0; i < BLOCK_CONTEXTS; i++)
            hdr->ref_delta[i] = bool_maybe_get_int(boolean_decoder, 6);

        for (i = 0; i < BLOCK_CONTEXTS; i++)
            hdr->mode_delta[i] = bool_maybe_get_int(boolean_decoder, 6);
    }
}


void
decode_segmentation_header(struct vp8_decoder_ctx *ctx,
                           struct bool_decoder    *boolean_decoder,
                           struct vp8_segment_hdr *hdr)
{
    if (ctx->frame_hdr.is_keyframe)
        memset(hdr, 0, sizeof(*hdr));

    hdr->enabled = bool_get_bit(boolean_decoder);

    if (hdr->enabled)
    {
        int i;

        hdr->update_map = bool_get_bit(boolean_decoder);
        hdr->update_data = bool_get_bit(boolean_decoder);

        if (hdr->update_data)
        {
            hdr->abs = bool_get_bit(boolean_decoder);

            for (i = 0; i < MAX_MB_SEGMENTS; i++)
                hdr->quant_idx[i] = bool_maybe_get_int(boolean_decoder, 7);

            for (i = 0; i < MAX_MB_SEGMENTS; i++)
                hdr->lf_level[i] = bool_maybe_get_int(boolean_decoder, 6);
        }

        if (hdr->update_map)
        {
            for (i = 0; i < MB_FEATURE_TREE_PROBS; i++)
                hdr->tree_probs[i] = bool_get_bit(boolean_decoder)
                                     ? bool_get_uint(boolean_decoder, 8)
                                     : 255;
        }
    }
    else
    {
        hdr->update_map = 0;
        hdr->update_data = 0;
    }
}


static void
dequant_global_init(struct dequant_factors dqf[MAX_MB_SEGMENTS])
{
    int i;

    for (i = 0; i < MAX_MB_SEGMENTS; i++)
        dqf[i].quant_idx = -1;
}


static int
clamp_q(int q)
{
    if (q < 0) return 0;
    else if (q > 127) return 127;

    return q;
}


static int
dc_q(int q)
{
    return dc_q_lookup[clamp_q(q)];
}


static int
ac_q(int q)
{
    return ac_q_lookup[clamp_q(q)];
}


static void
dequant_init(struct dequant_factors        factors[MAX_MB_SEGMENTS],
             const struct vp8_segment_hdr *seg,
             const struct vp8_quant_hdr   *quant_hdr)
{
    int i, q;
    struct dequant_factors *dqf = factors;

    for (i = 0; i < (seg->enabled ? MAX_MB_SEGMENTS : 1); i++)
    {
        q = quant_hdr->q_index;

        if (seg->enabled)
            q = (!seg->abs) ? q + seg->quant_idx[i] : seg->quant_idx[i];

        if (dqf->quant_idx != q || quant_hdr->delta_update)
        {
            dqf->factor[TOKEN_BLOCK_Y1][0] =
                dc_q(q + quant_hdr->y1_dc_delta_q);
            dqf->factor[TOKEN_BLOCK_Y1][1] =
                ac_q(q);
            dqf->factor[TOKEN_BLOCK_UV][0] =
                dc_q(q + quant_hdr->uv_dc_delta_q);
            dqf->factor[TOKEN_BLOCK_UV][1] =
                ac_q(q + quant_hdr->uv_ac_delta_q);
            dqf->factor[TOKEN_BLOCK_Y2][0] =
                dc_q(q + quant_hdr->y2_dc_delta_q) * 2;
            dqf->factor[TOKEN_BLOCK_Y2][1] =
                ac_q(q + quant_hdr->y2_ac_delta_q) * 155 / 100;

            if (dqf->factor[TOKEN_BLOCK_Y2][1] < 8)
                dqf->factor[TOKEN_BLOCK_Y2][1] = 8;

            if (dqf->factor[TOKEN_BLOCK_UV][0] > 132)
                dqf->factor[TOKEN_BLOCK_UV][0] = 132;

            dqf->quant_idx = q;
        }

        dqf++;
    }
}


void
decode_frame(struct vp8_decoder_ctx *ctx,
             const unsigned char    *data,
             unsigned int            sz)
{
    vpx_codec_err_t  res;
    struct bool_decoder  boolean_decoder;
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

    if (ctx->frame_hdr.is_keyframe)
    {
        data += KEYFRAME_HEADER_SZ;
        sz -= KEYFRAME_HEADER_SZ;
        ctx->mb_cols = (ctx->frame_hdr.kf.w + 15) / 16;
        ctx->mb_rows = (ctx->frame_hdr.kf.h + 15) / 16;
    }

    /* Start the bitreader for the header/entropy partition */
    init_bool_decoder(&boolean_decoder, data, ctx->frame_hdr.part0_sz);

    /* Skip the colorspace and clamping bits */
    if (ctx->frame_hdr.is_keyframe)
        if (bool_get_uint(&boolean_decoder, 2))
            vpx_internal_error(&ctx->error, VPX_CODEC_UNSUP_BITSTREAM,
                               "Reserved bits not supported.");

    decode_segmentation_header(ctx, &boolean_decoder, &ctx->segment_hdr);
    decode_loopfilter_header(ctx, &boolean_decoder, &ctx->loopfilter_hdr);
    decode_and_init_token_partitions(ctx,
                                     &boolean_decoder,
                                     data + ctx->frame_hdr.part0_sz,
                                     sz - ctx->frame_hdr.part0_sz,
                                     &ctx->token_hdr);
    decode_quantizer_header(ctx, &boolean_decoder, &ctx->quant_hdr);
    decode_reference_header(ctx, &boolean_decoder, &ctx->reference_hdr);

    /* Set keyframe entropy defaults. These get updated on keyframes
     * regardless of the refresh_entropy setting.
     */
    if (ctx->frame_hdr.is_keyframe)
    {
        ARRAY_COPY(ctx->entropy_hdr.coeff_probs,
                   k_default_coeff_probs);
        ARRAY_COPY(ctx->entropy_hdr.mv_probs,
                   k_default_mv_probs);
        ARRAY_COPY(ctx->entropy_hdr.y_mode_probs,
                   k_default_y_mode_probs);
        ARRAY_COPY(ctx->entropy_hdr.uv_mode_probs,
                   k_default_uv_mode_probs);
    }

    if (!ctx->reference_hdr.refresh_entropy)
    {
        ctx->saved_entropy = ctx->entropy_hdr;
        ctx->saved_entropy_valid = 1;
    }

    decode_entropy_header(ctx, &boolean_decoder, &ctx->entropy_hdr);

    vp8_dixie_modemv_init(ctx);
    vp8_dixie_tokens_init(ctx);
    vp8_dixie_predict_init(ctx);
    dequant_init(ctx->dequant_factors, &ctx->segment_hdr,
                 &ctx->quant_hdr);

    for (row = 0, partition = 0; row < ctx->mb_rows; row++)
    {
        vp8_dixie_modemv_process_row(ctx, &boolean_decoder, row, 0, ctx->mb_cols);
        vp8_dixie_tokens_process_row(ctx, partition, row, 0,
                                     ctx->mb_cols);
        vp8_dixie_predict_process_row(ctx, row, 0, ctx->mb_cols);

        if (ctx->loopfilter_hdr.level && row)
            vp8_dixie_loopfilter_process_row(ctx, row - 1, 0,
                                             ctx->mb_cols);

        if (++partition == ctx->token_hdr.partitions)
            partition = 0;
    }

    if (ctx->loopfilter_hdr.level)
        vp8_dixie_loopfilter_process_row(ctx, row - 1, 0, ctx->mb_cols);

    ctx->frame_cnt++;

    if (!ctx->reference_hdr.refresh_entropy)
    {
        ctx->entropy_hdr = ctx->saved_entropy;
        ctx->saved_entropy_valid = 0;
    }

    /* Handle reference frame updates */
    if (ctx->reference_hdr.copy_arf == 1)
    {
        vp8_dixie_release_ref_frame(ctx->ref_frames[ALTREF_FRAME]);
        ctx->ref_frames[ALTREF_FRAME] =
            vp8_dixie_ref_frame(ctx->ref_frames[LAST_FRAME]);
    }
    else if (ctx->reference_hdr.copy_arf == 2)
    {
        vp8_dixie_release_ref_frame(ctx->ref_frames[ALTREF_FRAME]);
        ctx->ref_frames[ALTREF_FRAME] =
            vp8_dixie_ref_frame(ctx->ref_frames[GOLDEN_FRAME]);
    }

    if (ctx->reference_hdr.copy_gf == 1)
    {
        vp8_dixie_release_ref_frame(ctx->ref_frames[GOLDEN_FRAME]);
        ctx->ref_frames[GOLDEN_FRAME] =
            vp8_dixie_ref_frame(ctx->ref_frames[LAST_FRAME]);
    }
    else if (ctx->reference_hdr.copy_gf == 2)
    {
        vp8_dixie_release_ref_frame(ctx->ref_frames[GOLDEN_FRAME]);
        ctx->ref_frames[GOLDEN_FRAME] =
            vp8_dixie_ref_frame(ctx->ref_frames[ALTREF_FRAME]);
    }

    if (ctx->reference_hdr.refresh_gf)
    {
        vp8_dixie_release_ref_frame(ctx->ref_frames[GOLDEN_FRAME]);
        ctx->ref_frames[GOLDEN_FRAME] =
            vp8_dixie_ref_frame(ctx->ref_frames[CURRENT_FRAME]);
    }

    if (ctx->reference_hdr.refresh_arf)
    {
        vp8_dixie_release_ref_frame(ctx->ref_frames[ALTREF_FRAME]);
        ctx->ref_frames[ALTREF_FRAME] =
            vp8_dixie_ref_frame(ctx->ref_frames[CURRENT_FRAME]);
    }

    if (ctx->reference_hdr.refresh_last)
    {
        vp8_dixie_release_ref_frame(ctx->ref_frames[LAST_FRAME]);
        ctx->ref_frames[LAST_FRAME] =
            vp8_dixie_ref_frame(ctx->ref_frames[CURRENT_FRAME]);
    }

}


void
vp8_dixie_decode_init(struct vp8_decoder_ctx *ctx)
{
    dequant_global_init(ctx->dequant_factors);
}


#define CHECK_FOR_UPDATE(lval,rval,update_flag) do {\
        unsigned int old = lval; \
        update_flag |= (old != (lval = rval)); \
    } while(0)

vpx_codec_err_t
vp8_parse_frame_header(const unsigned char   *data,
                       unsigned int           sz,
                       struct vp8_frame_hdr  *hdr)
{
    unsigned long raw;

    if (sz < 10)
        return VPX_CODEC_CORRUPT_FRAME;

    /* The frame header is defined as a three byte little endian
     * value
     */
    raw = data[0] | (data[1] << 8) | (data[2] << 16);
    hdr->is_keyframe     = !BITS_GET(raw, 0, 1);
    hdr->version         = BITS_GET(raw, 1, 2);
    hdr->is_experimental = BITS_GET(raw, 3, 1);
    hdr->is_shown        = BITS_GET(raw, 4, 1);
    hdr->part0_sz        = BITS_GET(raw, 5, 19);

    if (sz <= hdr->part0_sz + (hdr->is_keyframe ? 10 : 3))
        return VPX_CODEC_CORRUPT_FRAME;

    hdr->frame_size_updated = 0;

    if (hdr->is_keyframe)
    {
        unsigned int update = 0;

        /* Keyframe header consists of a three byte sync code followed
         * by the width and height and associated scaling factors.
         */
        if (data[3] != 0x9d || data[4] != 0x01 || data[5] != 0x2a)
            return VPX_CODEC_UNSUP_BITSTREAM;

        raw = data[6] | (data[7] << 8)
              | (data[8] << 16) | (data[9] << 24);
        CHECK_FOR_UPDATE(hdr->kf.w,       BITS_GET(raw,  0, 14),
                         update);
        CHECK_FOR_UPDATE(hdr->kf.scale_w, BITS_GET(raw, 14,  2),
                         update);
        CHECK_FOR_UPDATE(hdr->kf.h,       BITS_GET(raw, 16, 14),
                         update);
        CHECK_FOR_UPDATE(hdr->kf.scale_h, BITS_GET(raw, 30,  2),
                         update);

        hdr->frame_size_updated = update;

        if (!hdr->kf.w || !hdr->kf.h)
            return VPX_CODEC_UNSUP_BITSTREAM;
    }

    return VPX_CODEC_OK;
}


vpx_codec_err_t
vp8_dixie_decode_frame(struct vp8_decoder_ctx *ctx,
                       const unsigned char    *data,
                       unsigned int            sz)
{
    volatile struct vp8_decoder_ctx *ctx_ = ctx;

    ctx->error.error_code = VPX_CODEC_OK;
    ctx->error.has_detail = 0;

    if (!setjmp(ctx->error.jmp))
        decode_frame(ctx, data, sz);

    return ctx_->error.error_code;
}


void
vp8_dixie_decode_destroy(struct vp8_decoder_ctx *ctx)
{
    vp8_dixie_predict_destroy(ctx);
    vp8_dixie_tokens_destroy(ctx);
    vp8_dixie_modemv_destroy(ctx);
}
