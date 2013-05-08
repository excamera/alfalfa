/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "dixie.h"
#include "dixie_loopfilter.h"

#define ABS(x) ((x) >= 0 ? (x) : -(x))

#define p3 pixels[-4*stride]
#define p2 pixels[-3*stride]
#define p1 pixels[-2*stride]
#define p0 pixels[-1*stride]
#define q0 pixels[ 0*stride]
#define q1 pixels[ 1*stride]
#define q2 pixels[ 2*stride]
#define q3 pixels[ 3*stride]

#define static
static int
saturate_int8(int x)
{
    if (x < -128)
        return -128;

    if (x > 127)
        return 127;

    return x;
}


static int
saturate_uint8(int x)
{
    if (x < 0)
        return 0;

    if (x > 255)
        return 255;

    return x;
}


static int
high_edge_variance(unsigned char *pixels,
                   int            stride,
                   int            hev_threshold)
{
    return ABS(p1 - p0) > hev_threshold || ABS(q1 - q0) > hev_threshold;
}


static int
simple_threshold(unsigned char *pixels,
                 int            stride,
                 int            filter_limit)
{
    return (ABS(p0 - q0) * 2 + (ABS(p1 - q1) >> 1)) <= filter_limit;
}


static int
normal_threshold(unsigned char *pixels,
                 int            stride,
                 int            edge_limit,
                 int            interior_limit)
{
    int E = edge_limit;
    int I = interior_limit;

    return simple_threshold(pixels, stride, 2 * E + I)
           && ABS(p3 - p2) <= I && ABS(p2 - p1) <= I
           && ABS(p1 - p0) <= I && ABS(q3 - q2) <= I
           && ABS(q2 - q1) <= I && ABS(q1 - q0) <= I;
}


static void
filter_common(unsigned char *pixels,
              int            stride,
              int            use_outer_taps)
{
    int a, f1, f2;

    a = 3 * (q0 - p0);

    if (use_outer_taps)
        a += saturate_int8(p1 - q1);

    a = saturate_int8(a);

    f1 = ((a + 4 > 127) ? 127 : a + 4) >> 3;
    f2 = ((a + 3 > 127) ? 127 : a + 3) >> 3;

    p0 = saturate_uint8(p0 + f2);
    q0 = saturate_uint8(q0 - f1);

    if (!use_outer_taps)
    {
        /* This handles the case of subblock_filter()
         * (from the bitstream guide.
         */
        a = (f1 + 1) >> 1;
        p1 = saturate_uint8(p1 + a);
        q1 = saturate_uint8(q1 - a);
    }
}


static void
filter_mb_edge(unsigned char *pixels,
               int            stride)
{
    int w, a;

    w = saturate_int8(saturate_int8(p1 - q1) + 3 * (q0 - p0));

    a = (27 * w + 63) >> 7;
    p0 = saturate_uint8(p0 + a);
    q0 = saturate_uint8(q0 - a);

    a = (18 * w + 63) >> 7;
    p1 = saturate_uint8(p1 + a);
    q1 = saturate_uint8(q1 - a);

    a = (9 * w + 63) >> 7;
    p2 = saturate_uint8(p2 + a);
    q2 = saturate_uint8(q2 - a);

}


static void
filter_mb_v_edge(unsigned char *src,
                 int            stride,
                 int            edge_limit,
                 int            interior_limit,
                 int            hev_threshold,
                 int            size)
{
    int i;

    for (i = 0; i < 8 * size; i++)
    {
        if (normal_threshold(src, 1, edge_limit, interior_limit))
        {
            if (high_edge_variance(src, 1, hev_threshold))
                filter_common(src, 1, 1);
            else
                filter_mb_edge(src, 1);
        }

        src += stride;
    }
}


static void
filter_subblock_v_edge(unsigned char *src,
                       int            stride,
                       int            edge_limit,
                       int            interior_limit,
                       int            hev_threshold,
                       int            size)
{
    int i;

    for (i = 0; i < 8 * size; i++)
    {
        if (normal_threshold(src, 1, edge_limit, interior_limit))
            filter_common(src, 1,
                          high_edge_variance(src, 1, hev_threshold));

        src += stride;
    }
}


static void
filter_mb_h_edge(unsigned char *src,
                 int            stride,
                 int            edge_limit,
                 int            interior_limit,
                 int            hev_threshold,
                 int            size)
{
    int i;

    for (i = 0; i < 8 * size; i++)
    {
        if (normal_threshold(src, stride, edge_limit, interior_limit))
        {
            if (high_edge_variance(src, stride, hev_threshold))
                filter_common(src, stride, 1);
            else
                filter_mb_edge(src, stride);
        }

        src += 1;
    }
}


static void
filter_subblock_h_edge(unsigned char *src,
                       int            stride,
                       int            edge_limit,
                       int            interior_limit,
                       int            hev_threshold,
                       int            size)
{
    int i;

    for (i = 0; i < 8 * size; i++)
    {
        if (normal_threshold(src, stride, edge_limit, interior_limit))
            filter_common(src, stride,
                          high_edge_variance(src, stride,
                                             hev_threshold));

        src += 1;
    }
}


static void
filter_v_edge_simple(unsigned char *src,
                     int            stride,
                     int            filter_limit)
{
    int i;

    for (i = 0; i < 16; i++)
    {
        if (simple_threshold(src, 1, filter_limit))
            filter_common(src, 1, 1);

        src += stride;
    }
}


static void
filter_h_edge_simple(unsigned char *src,
                     int            stride,
                     int            filter_limit)
{
    int i;

    for (i = 0; i < 16; i++)
    {
        if (simple_threshold(src, stride, filter_limit))
            filter_common(src, stride, 1);

        src += 1;
    }
}


static void
calculate_filter_parameters(struct vp8_decoder_ctx *ctx,
                            struct mb_info         *mbi,
                            int                    *edge_limit_,
                            int                    *interior_limit_,
                            int                    *hev_threshold_)
{
    int filter_level, interior_limit, hev_threshold;

    /* Reference code/spec seems to conflate filter_level and
     * edge_limit
     */

    filter_level = ctx->loopfilter_hdr.level;

    if (ctx->segment_hdr.enabled)
    {
        if (!ctx->segment_hdr.abs)
            filter_level +=
                ctx->segment_hdr.lf_level[mbi->base.segment_id];
        else
            filter_level =
                ctx->segment_hdr.lf_level[mbi->base.segment_id];
    }

    if (ctx->loopfilter_hdr.delta_enabled)
    {
        filter_level +=
            ctx->loopfilter_hdr.ref_delta[mbi->base.ref_frame];

        if (mbi->base.ref_frame == CURRENT_FRAME)
        {
            if (mbi->base.y_mode == B_PRED)
                filter_level += ctx->loopfilter_hdr.mode_delta[0];
        }
        else if (mbi->base.y_mode == ZEROMV)
            filter_level += ctx->loopfilter_hdr.mode_delta[1];
        else if (mbi->base.y_mode == SPLITMV)
            filter_level += ctx->loopfilter_hdr.mode_delta[3];
        else
            filter_level += ctx->loopfilter_hdr.mode_delta[2];
    }

    if (filter_level > 63)
        filter_level = 63;
    else if (filter_level < 0)
        filter_level = 0;

    interior_limit = filter_level;

    if (ctx->loopfilter_hdr.sharpness)
    {
        interior_limit >>= ctx->loopfilter_hdr.sharpness > 4 ? 2 : 1;

        if (interior_limit > 9 - ctx->loopfilter_hdr.sharpness)
            interior_limit = 9 - ctx->loopfilter_hdr.sharpness;
    }

    if (interior_limit < 1)
        interior_limit = 1;

    hev_threshold = (filter_level >= 15);

    if (filter_level >= 40)
        hev_threshold++;

    if (filter_level >= 20 && !ctx->frame_hdr.is_keyframe)
        hev_threshold++;

    *edge_limit_ = filter_level;
    *interior_limit_ = interior_limit;
    *hev_threshold_ = hev_threshold;
}


static void
filter_row_normal(struct vp8_decoder_ctx *ctx,
                  unsigned int            row,
                  unsigned int            start_col,
                  unsigned int            num_cols)
{
    unsigned char  *y, *u, *v;
    int             stride, uv_stride;
    struct mb_info *mbi;
    unsigned int    col;

    /* Adjust pointers based on row, start_col */
    stride    = ctx->ref_frames[CURRENT_FRAME]->img.stride[PLANE_Y];
    uv_stride = ctx->ref_frames[CURRENT_FRAME]->img.stride[PLANE_U];
    y = ctx->ref_frames[CURRENT_FRAME]->img.planes[PLANE_Y];
    u = ctx->ref_frames[CURRENT_FRAME]->img.planes[PLANE_U];
    v = ctx->ref_frames[CURRENT_FRAME]->img.planes[PLANE_V];
    y += (stride * row + start_col) * 16;
    u += (uv_stride * row + start_col) * 8;
    v += (uv_stride * row + start_col) * 8;
    mbi = ctx->mb_info_rows[row] + start_col;

    for (col = start_col; col < start_col + num_cols; col++)
    {
        int edge_limit, interior_limit, hev_threshold;

        /* TODO: only need to recalculate every MB if segmentation is
         * enabled.
         */
        calculate_filter_parameters(ctx, mbi, &edge_limit,
                                    &interior_limit, &hev_threshold);

        if (edge_limit)
        {
            if (col)
            {
                filter_mb_v_edge(y, stride, edge_limit + 2,
                                 interior_limit, hev_threshold, 2);
                filter_mb_v_edge(u, uv_stride, edge_limit + 2,
                                 interior_limit, hev_threshold, 1);
                filter_mb_v_edge(v, uv_stride, edge_limit + 2,
                                 interior_limit, hev_threshold, 1);
            }

            /* NOTE: This conditional is actually dependent on the
             * number of coefficients decoded, not the skip flag as
             * coded in the bitstream. The tokens task is expected to
             * set 31 if there is *any* non-zero data.
             */
            if (mbi->base.eob_mask
                || mbi->base.y_mode == SPLITMV
                || mbi->base.y_mode == B_PRED)
            {
                filter_subblock_v_edge(y + 4, stride, edge_limit,
                                       interior_limit, hev_threshold,
                                       2);
                filter_subblock_v_edge(y + 8, stride, edge_limit,
                                       interior_limit, hev_threshold,
                                       2);
                filter_subblock_v_edge(y + 12, stride, edge_limit,
                                       interior_limit, hev_threshold,
                                       2);
                filter_subblock_v_edge(u + 4, uv_stride, edge_limit,
                                       interior_limit, hev_threshold,
                                       1);
                filter_subblock_v_edge(v + 4, uv_stride, edge_limit,
                                       interior_limit, hev_threshold,
                                       1);
            }

            if (row)
            {
                filter_mb_h_edge(y, stride, edge_limit + 2,
                                 interior_limit, hev_threshold, 2);
                filter_mb_h_edge(u, uv_stride, edge_limit + 2,
                                 interior_limit, hev_threshold, 1);
                filter_mb_h_edge(v, uv_stride, edge_limit + 2,
                                 interior_limit, hev_threshold, 1);
            }

            if (mbi->base.eob_mask
                || mbi->base.y_mode == SPLITMV
                || mbi->base.y_mode == B_PRED)
            {
                filter_subblock_h_edge(y + 4 * stride, stride,
                                       edge_limit, interior_limit,
                                       hev_threshold, 2);
                filter_subblock_h_edge(y + 8 * stride, stride,
                                       edge_limit, interior_limit,
                                       hev_threshold, 2);
                filter_subblock_h_edge(y + 12 * stride, stride,
                                       edge_limit, interior_limit,
                                       hev_threshold, 2);
                filter_subblock_h_edge(u + 4 * uv_stride, uv_stride,
                                       edge_limit, interior_limit,
                                       hev_threshold, 1);
                filter_subblock_h_edge(v + 4 * uv_stride, uv_stride,
                                       edge_limit, interior_limit,
                                       hev_threshold, 1);
            }
        }

        y += 16;
        u += 8;
        v += 8;
        mbi++;
    }
}


static void
filter_row_simple(struct vp8_decoder_ctx *ctx,
                  unsigned int            row,
                  unsigned int            start_col,
                  unsigned int            num_cols)
{
    unsigned char  *y;
    int             stride;
    struct mb_info *mbi;
    unsigned int    col;

    /* Adjust pointers based on row, start_col */
    stride    = ctx->ref_frames[CURRENT_FRAME]->img.stride[PLANE_Y];
    y = ctx->ref_frames[CURRENT_FRAME]->img.planes[PLANE_Y];
    y += (stride * row + start_col) * 16;
    mbi = ctx->mb_info_rows[row] + start_col;

    for (col = start_col; col < start_col + num_cols; col++)
    {
        int edge_limit, interior_limit, hev_threshold;

        /* TODO: only need to recalculate every MB if segmentation is
         * enabled.
         */
        calculate_filter_parameters(ctx, mbi, &edge_limit,
                                    &interior_limit, &hev_threshold);

        if (edge_limit)
        {

            /* NOTE: This conditional is actually dependent on the
             * number of coefficients decoded, not the skip flag as
             * coded in the bitstream. The tokens task is expected to
             * set 31 if there is *any* non-zero data.
             */
            int filter_subblocks = (mbi->base.eob_mask
                                    || mbi->base.y_mode == SPLITMV
                                    || mbi->base.y_mode == B_PRED);
            int mb_limit = (edge_limit + 2) * 2 + interior_limit;
            int b_limit = edge_limit * 2 + interior_limit;

            if (col)
                filter_v_edge_simple(y, stride, mb_limit);

            if (filter_subblocks)
            {
                filter_v_edge_simple(y + 4, stride, b_limit);
                filter_v_edge_simple(y + 8, stride, b_limit);
                filter_v_edge_simple(y + 12, stride, b_limit);
            }

            if (row)
                filter_h_edge_simple(y, stride, mb_limit);

            if (filter_subblocks)
            {
                filter_h_edge_simple(y + 4 * stride, stride, b_limit);
                filter_h_edge_simple(y + 8 * stride, stride, b_limit);
                filter_h_edge_simple(y + 12 * stride, stride, b_limit);
            }
        }

        y += 16;
        mbi++;
    }
}


void
vp8_dixie_loopfilter_process_row(struct vp8_decoder_ctx *ctx,
                                 unsigned int            row,
                                 unsigned int            start_col,
                                 unsigned int            num_cols)
{
    if (ctx->loopfilter_hdr.use_simple)
        filter_row_simple(ctx, row, start_col, num_cols);
    else
        filter_row_normal(ctx, row, start_col, num_cols);
}
