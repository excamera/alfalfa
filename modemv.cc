/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "dixie.hh"
#include "modemv_data.hh"
#include <stdlib.h>
#include <assert.h>
#include "modemv.hh"

static union mv
        clamp_mv(union mv raw, const struct mv_clamp_rect *bounds)
{
    union mv newmv;

    newmv.d.x = (raw.d.x < bounds->to_left)
                ? bounds->to_left : raw.d.x;
    newmv.d.x = (raw.d.x > bounds->to_right)
                ? bounds->to_right : newmv.d.x;
    newmv.d.y = (raw.d.y < bounds->to_top)
                ? bounds->to_top : raw.d.y;
    newmv.d.y = (raw.d.y > bounds->to_bottom)
                ? bounds->to_bottom : newmv.d.y;
    return newmv;
}


int
read_segment_id(struct bool_decoder *boolean_decoder, struct vp8_segment_hdr *seg)
{
    return bool_get(boolean_decoder, seg->tree_probs[0])
           ? 2 + bool_get(boolean_decoder, seg->tree_probs[2])
           : bool_get(boolean_decoder, seg->tree_probs[1]);
}


static enum prediction_mode
above_block_mode(const struct mb_info *current_mb,
                 const struct mb_info *above,
                 unsigned int b)
{
    if (b < 4)
    {
        switch (above->base.y_mode)
        {
        case DC_PRED:
            return B_DC_PRED;
        case V_PRED:
            return B_VE_PRED;
        case H_PRED:
            return B_HE_PRED;
        case TM_PRED:
            return B_TM_PRED;
        case B_PRED:
            return above->split.modes[b+12];
        default:
            assert(0);
        }
    }

    return current_mb->split.modes[b-4];
}


static enum prediction_mode
left_block_mode(const struct mb_info *current_mb,
                const struct mb_info *left,
                unsigned int b)
{
    if (!(b & 3))
    {
        switch (left->base.y_mode)
        {
        case DC_PRED:
            return B_DC_PRED;
        case V_PRED:
            return B_VE_PRED;
        case H_PRED:
            return B_HE_PRED;
        case TM_PRED:
            return B_TM_PRED;
        case B_PRED:
            return left->split.modes[b+3];
        default:
            assert(0);
        }
    }

    return current_mb->split.modes[b-1];
}


static void
decode_kf_mb_mode(struct mb_info      *current_mb,
                  struct mb_info      *left,
                  struct mb_info      *above,
                  struct bool_decoder *boolean_decoder)
{
    prediction_mode y_mode, uv_mode;

    y_mode = static_cast<prediction_mode>( bool_read_tree(boolean_decoder, kf_y_mode_tree, kf_y_mode_probs) );

    if (y_mode == B_PRED)
    {
        unsigned int i;

        for (i = 0; i < 16; i++)
        {
            enum prediction_mode a = above_block_mode(current_mb, above, i);
            enum prediction_mode l = left_block_mode(current_mb, left, i);
            enum prediction_mode b;

            b = static_cast<prediction_mode>( bool_read_tree(boolean_decoder, b_mode_tree,
							     kf_b_mode_probs[a][l]) );
            current_mb->split.modes[i] = b;
        }
    }

    uv_mode = static_cast<prediction_mode>( bool_read_tree(boolean_decoder, uv_mode_tree, kf_uv_mode_probs) );

    current_mb->base.y_mode = y_mode;
    current_mb->base.uv_mode = uv_mode;
    current_mb->base.mv.raw = 0;
    current_mb->base.ref_frame = 0;
}


void
decode_intra_mb_mode(struct mb_info         *current_mb,
                     struct vp8_entropy_hdr *hdr,
                     struct bool_decoder    *boolean_decoder)
{
    /* Like decode_kf_mb_mode, but with probabilities transmitted in the
     * bitstream and no context on the above/left block mode.
     */
    prediction_mode y_mode, uv_mode;

    y_mode = static_cast<prediction_mode>( bool_read_tree(boolean_decoder, y_mode_tree, hdr->y_mode_probs) );

    if (y_mode == B_PRED)
    {
        unsigned int i;

        for (i = 0; i < 16; i++)
        {
            enum prediction_mode b;

            b = static_cast<prediction_mode>( bool_read_tree(boolean_decoder, b_mode_tree, default_b_mode_probs) );
            current_mb->split.modes[i] = b;
        }
    }

    uv_mode = static_cast<prediction_mode>( bool_read_tree(boolean_decoder, uv_mode_tree, hdr->uv_mode_probs) );

    current_mb->base.y_mode = y_mode;
    current_mb->base.uv_mode = uv_mode;
    current_mb->base.mv.raw = 0;
    current_mb->base.ref_frame = CURRENT_FRAME;
}


static int
read_mv_component(struct bool_decoder *boolean_decoder,
                  const unsigned char  mvc[MV_PROB_CNT])
{
    enum {IS_SHORT, SIGN, SHORT, BITS = SHORT + 8 - 1, LONG_WIDTH = 10};
    int x = 0;

    if (bool_get(boolean_decoder, mvc[IS_SHORT])) /* Large */
    {
        int i = 0;

        for (i = 0; i < 3; i++)
            x += bool_get(boolean_decoder, mvc[BITS + i]) << i;

        /* Skip bit 3, which is sometimes implicit */
        for (i = LONG_WIDTH - 1; i > 3; i--)
            x += bool_get(boolean_decoder, mvc[BITS + i]) << i;

        if (!(x & 0xFFF0)  ||  bool_get(boolean_decoder, mvc[BITS + 3]))
            x += 8;
    }
    else   /* small */
        x = bool_read_tree(boolean_decoder, small_mv_tree, mvc + SHORT);

    if (x && bool_get(boolean_decoder, mvc[SIGN]))
        x = -x;

    return x << 1;
}


static mv_t
above_block_mv(const struct mb_info *current_mb,
               const struct mb_info *above,
               unsigned int          b)
{
    if (b < 4)
    {
        if (above->base.y_mode == SPLITMV)
            return above->split.mvs[b+12];

        return above->base.mv;
    }

    return current_mb->split.mvs[b-4];
}


static mv_t
left_block_mv(const struct mb_info *current_mb,
              const struct mb_info *left,
              unsigned int          b)
{
    if (!(b & 3))
    {
        if (left->base.y_mode == SPLITMV)
            return left->split.mvs[b+3];

        return left->base.mv;
    }

    return current_mb->split.mvs[b-1];
}


static enum prediction_mode
submv_ref(struct bool_decoder *boolean_decoder, union mv l, union mv a)
{
    enum subblock_mv_ref
    {
        SUBMVREF_NORMAL,
        SUBMVREF_LEFT_ZED,
        SUBMVREF_ABOVE_ZED,
        SUBMVREF_LEFT_ABOVE_SAME,
        SUBMVREF_LEFT_ABOVE_ZED
    };

    int lez = !(l.raw);
    int aez = !(a.raw);
    int lea = l.raw == a.raw;
    enum subblock_mv_ref ctx = SUBMVREF_NORMAL;

    if (lea && lez)
        ctx = SUBMVREF_LEFT_ABOVE_ZED;
    else if (lea)
        ctx = SUBMVREF_LEFT_ABOVE_SAME;
    else if (aez)
        ctx = SUBMVREF_ABOVE_ZED;
    else if (lez)
        ctx = SUBMVREF_LEFT_ZED;

    return static_cast<prediction_mode>( bool_read_tree(boolean_decoder, submv_ref_tree, submv_ref_probs2[ctx]) );
}


static void
read_mv(struct bool_decoder  *boolean_decoder,
        union mv             *mv,
        mv_component_probs_t  mvc[2])
{
    mv->d.y = read_mv_component(boolean_decoder, mvc[0]);
    mv->d.x = read_mv_component(boolean_decoder, mvc[1]);
}


static void
mv_bias(const struct mb_info *mb,
        const unsigned int   sign_bias[3],
        enum reference_frame ref_frame,
        union mv             *mv)
{
    if (sign_bias[mb->base.ref_frame] ^ sign_bias[ref_frame])
    {
        mv->d.x *= -1;
        mv->d.y *= -1;
    }
}


enum near_mv_v
{
    CNT_BEST = 0,
    CNT_ZEROZERO = 0,
    CNT_NEAREST,
    CNT_NEAR,
    CNT_SPLITMV
};


static void
find_near_mvs(const struct mb_info   *current_mb,
              const struct mb_info   *left,
              const struct mb_info   *above,
              const unsigned int      sign_bias[3],
              union  mv               near_mvs[4],
              int                     cnt[4])
{
    const struct mb_info *aboveleft = above - 1;
    union  mv             *mv = near_mvs;
    int                   *cntx = cnt;

    /* Zero accumulators */
    mv[0].raw = mv[1].raw = mv[2].raw = 0;
    cnt[0] = cnt[1] = cnt[2] = cnt[3] = 0;

    /* Process above */
    if (above->base.ref_frame != CURRENT_FRAME)
    {
        if (above->base.mv.raw)
        {
            (++mv)->raw = above->base.mv.raw;
            mv_bias(above, sign_bias, static_cast<reference_frame>( current_mb->base.ref_frame ), mv);
            ++cntx;
        }

        *cntx += 2;
    }

    /* Process left */
    if (left->base.ref_frame != CURRENT_FRAME)
    {
        if (left->base.mv.raw)
        {
            union mv this_mv;

            this_mv.raw = left->base.mv.raw;
            mv_bias(left, sign_bias, static_cast<reference_frame>( current_mb->base.ref_frame ), &this_mv);

            if (this_mv.raw != mv->raw)
            {
                (++mv)->raw = this_mv.raw;
                ++cntx;
            }

            *cntx += 2;
        }
        else
            cnt[CNT_ZEROZERO] += 2;
    }

    /* Process above left */
    if (aboveleft->base.ref_frame != CURRENT_FRAME)
    {
        if (aboveleft->base.mv.raw)
        {
            union mv this_mv;

            this_mv.raw = aboveleft->base.mv.raw;
            mv_bias(aboveleft, sign_bias, static_cast<reference_frame>( current_mb->base.ref_frame ),
                    &this_mv);

            if (this_mv.raw != mv->raw)
            {
                (++mv)->raw = this_mv.raw;
                ++cntx;
            }

            *cntx += 1;
        }
        else
            cnt[CNT_ZEROZERO] += 1;
    }

    /* If we have three distinct MV's ... */
    if (cnt[CNT_SPLITMV])
    {
        /* See if above-left MV can be merged with NEAREST */
        if (mv->raw == near_mvs[CNT_NEAREST].raw)
            cnt[CNT_NEAREST] += 1;
    }

    cnt[CNT_SPLITMV] = ((above->base.y_mode == SPLITMV)
                        + (left->base.y_mode == SPLITMV)) * 2
                       + (aboveleft->base.y_mode == SPLITMV);

    /* Swap near and nearest if necessary */
    if (cnt[CNT_NEAR] > cnt[CNT_NEAREST])
    {
        int tmp;
        tmp = cnt[CNT_NEAREST];
        cnt[CNT_NEAREST] = cnt[CNT_NEAR];
        cnt[CNT_NEAR] = tmp;
        tmp = near_mvs[CNT_NEAREST].raw;
        near_mvs[CNT_NEAREST].raw = near_mvs[CNT_NEAR].raw;
        near_mvs[CNT_NEAR].raw = tmp;
    }

    /* Use near_mvs[CNT_BEST] to store the "best" MV. Note that this
     * storage shares the same address as near_mvs[CNT_ZEROZERO].
     */
    if (cnt[CNT_NEAREST] >= cnt[CNT_BEST])
        near_mvs[CNT_BEST] = near_mvs[CNT_NEAREST];
}


static void
decode_split_mv(struct mb_info         *current_mb,
                const struct mb_info   *left,
                const struct mb_info   *above,
                struct vp8_entropy_hdr *hdr,
                union  mv              *best_mv,
                struct bool_decoder    *boolean_decoder)
{
    const int *partition;
    int        j, k, mask;
    splitmv_partitioning partition_id;

    partition_id = static_cast<splitmv_partitioning>( bool_read_tree(boolean_decoder, split_mv_tree, split_mv_probs) );
    partition = mv_partitions[partition_id];
    current_mb->base.partitioning = partition_id;

    for (j = 0, mask = 0; mask < 65535; j++)
    {
        union mv mv, left_mv, above_mv;
        enum prediction_mode subblock_mode;

        /* Find the first subblock in this partition. */
        for (k = 0; j != partition[k]; k++);

        /* Decode the next MV */
        left_mv = left_block_mv(current_mb, left, k);
        above_mv = above_block_mv(current_mb, above, k);
        subblock_mode = submv_ref(boolean_decoder, left_mv,  above_mv);

        switch (subblock_mode)
        {
        case LEFT4X4:
            mv = left_mv;
            break;
        case ABOVE4X4:
            mv = above_mv;
            break;
        case ZERO4X4:
            mv.raw = 0;
            break;
        case NEW4X4:
            read_mv(boolean_decoder, &mv, hdr->mv_probs);
            mv.d.x += best_mv->d.x;
            mv.d.y += best_mv->d.y;
            break;
        default:
            assert(0);
        }

        /* Fill the MV's for this partition */
        for (; k < 16; k++)
            if (j == partition[k])
            {
                current_mb->split.mvs[k] = mv;
                mask |= 1 << k;
            }
    }
}


static int
need_mc_border(union mv mv, int l, int t, int b_w, int w, int h)
{
    int b, r;

    /* Get distance to edge for top-left pixel */
    l += (mv.d.x >> 3);
    t += (mv.d.y >> 3);

    /* Get distance to edge for bottom-right pixel */
    r = w - (l + b_w);
    b = h - (t + b_w);

    return (l >> 1 < 2 || r >> 1 < 3 || t >> 1 < 2 || b >> 1 < 3);
}

void
decode_mvs(struct vp8_decoder_ctx       *ctx,
           struct mb_info               *current_mb,
           const struct mb_info         *left,
           const struct mb_info         *above,
           const struct mv_clamp_rect   *bounds,
           struct bool_decoder          *boolean_decoder)
{
    struct vp8_entropy_hdr *hdr = &ctx->entropy_hdr;
    union mv          near_mvs[4];
    union mv          clamped_best_mv;
    int               mv_cnts[4];
    unsigned char     probs[4];
    enum {BEST, NEAREST, NEAR};
    int x, y, w, h, b;

    current_mb->base.ref_frame = bool_get(boolean_decoder, hdr->prob_last)
                           ? 2 + bool_get(boolean_decoder, hdr->prob_gf)
                           : 1;

    find_near_mvs(current_mb, current_mb - 1, above, ctx->reference_hdr.sign_bias,
                  near_mvs, mv_cnts);
    probs[0] = mv_counts_to_probs[mv_cnts[0]][0];
    probs[1] = mv_counts_to_probs[mv_cnts[1]][1];
    probs[2] = mv_counts_to_probs[mv_cnts[2]][2];
    probs[3] = mv_counts_to_probs[mv_cnts[3]][3];

    current_mb->base.y_mode = static_cast<prediction_mode>( bool_read_tree(boolean_decoder, mv_ref_tree, probs) );
    current_mb->base.uv_mode = current_mb->base.y_mode;

    current_mb->base.need_mc_border = 0;
    x = (-bounds->to_left - 128) >> 3;
    y = (-bounds->to_top - 128) >> 3;
    w = ctx->mb_cols * 16;
    h = ctx->mb_rows * 16;

    switch (current_mb->base.y_mode)
    {
    case NEARESTMV:
        current_mb->base.mv = clamp_mv(near_mvs[NEAREST], bounds);
        break;
    case NEARMV:
        current_mb->base.mv = clamp_mv(near_mvs[NEAR], bounds);
        break;
    case ZEROMV:
        current_mb->base.mv.raw = 0;
        return; //skip need_mc_border check
    case NEWMV:
        clamped_best_mv = clamp_mv(near_mvs[BEST], bounds);
        read_mv(boolean_decoder, &current_mb->base.mv, hdr->mv_probs);
        current_mb->base.mv.d.x += clamped_best_mv.d.x;
        current_mb->base.mv.d.y += clamped_best_mv.d.y;
        break;
    case SPLITMV:
    {
        union mv          chroma_mv[4] = {};

        clamped_best_mv = clamp_mv(near_mvs[BEST], bounds);
        decode_split_mv(current_mb, left, above, hdr, &clamped_best_mv, boolean_decoder);
        current_mb->base.mv = current_mb->split.mvs[15];

        for (b = 0; b < 16; b++)
        {
            chroma_mv[(b>>1&1) + (b>>2&2)].d.x +=
                current_mb->split.mvs[b].d.x;
            chroma_mv[(b>>1&1) + (b>>2&2)].d.y +=
                current_mb->split.mvs[b].d.y;

            if (need_mc_border(current_mb->split.mvs[b],
            x + (b & 3) * 4, y + (b & ~3), 4, w, h))
            {
                current_mb->base.need_mc_border = 1;
                break;
            }
        }

        for (b = 0; b < 4; b++)
        {
            chroma_mv[b].d.x += 4 + 8 * (chroma_mv[b].d.x >> 31);
            chroma_mv[b].d.y += 4 + 8 * (chroma_mv[b].d.y >> 31);
            chroma_mv[b].d.x /= 4;
            chroma_mv[b].d.y /= 4;

            //note we're passing in non-subsampled coordinates
            if (need_mc_border(chroma_mv[b],
            x + (b & 1) * 8, y + (b >> 1) * 8, 16, w, h))
            {
                current_mb->base.need_mc_border = 1;
                break;
            }
        }

        return; //skip need_mc_border check
    }
    default:
        assert(0);
    }

    if (need_mc_border(current_mb->base.mv, x, y, 16, w, h))
        current_mb->base.need_mc_border = 1;
}


void
vp8_dixie_modemv_process_row(struct vp8_decoder_ctx *ctx,
struct bool_decoder    *boolean_decoder,
int                     row,
unsigned int            start_col,
unsigned int            num_cols)
{
    struct mb_info       *above, *current_mb;
    unsigned int          col;
    struct mv_clamp_rect  bounds;

    current_mb = ctx->mb_info_rows[row] + start_col;
    above = ctx->mb_info_rows[row - 1] + start_col;

    /* Calculate the eighth-pel MV bounds using a 1 MB border. */
    bounds.to_left   = -((start_col + 1) << 7);
    bounds.to_right  = (ctx->mb_cols - start_col) << 7;
    bounds.to_top    = -((row + 1) << 7);
    bounds.to_bottom = (ctx->mb_rows - row) << 7;

    for (col = start_col; col < start_col + num_cols; col++)
    {
        if (ctx->segment_hdr.update_map)
            current_mb->base.segment_id = read_segment_id(boolean_decoder,
            &ctx->segment_hdr);

        if (ctx->entropy_hdr.coeff_skip_enabled)
            current_mb->base.skip_coeff = bool_get(boolean_decoder,
            ctx->entropy_hdr.coeff_skip_prob);

        if (ctx->frame_hdr.is_keyframe)
        {
            if (!ctx->segment_hdr.update_map)
                current_mb->base.segment_id = 0;

            decode_kf_mb_mode(current_mb, current_mb - 1, above, boolean_decoder);
        }
        else
        {
            if (bool_get(boolean_decoder, ctx->entropy_hdr.prob_inter))
                decode_mvs(ctx, current_mb, current_mb - 1, above, &bounds, boolean_decoder);
            else
                decode_intra_mb_mode(current_mb, &ctx->entropy_hdr, boolean_decoder);

            bounds.to_left -= 16 << 3;
            bounds.to_right -= 16 << 3;
        }

        /* Advance to next mb */
        current_mb++;
        above++;
    }
}


void
vp8_dixie_modemv_init(struct vp8_decoder_ctx *ctx)
{
    unsigned int    mbi_w, mbi_h, i;
    struct mb_info *mbi;

    mbi_w = ctx->mb_cols + 1; /* For left border col */
    mbi_h = ctx->mb_rows + 1; /* For above border row */

    if (ctx->frame_hdr.frame_size_updated)
    {
        free(ctx->mb_info_storage);
        ctx->mb_info_storage = NULL;
        free(ctx->mb_info_rows_storage);
        ctx->mb_info_rows_storage = NULL;
    }

    if (!ctx->mb_info_storage)
      ctx->mb_info_storage = static_cast<mb_info *>( calloc(mbi_w * mbi_h,
							    sizeof(*ctx->mb_info_storage)) );

    if (!ctx->mb_info_rows_storage)
      ctx->mb_info_rows_storage = static_cast<mb_info **>( calloc(mbi_h,
								  sizeof(*ctx->mb_info_rows_storage)) );

    /* Set up row pointers */
    mbi = ctx->mb_info_storage + 1;

    for (i = 0; i < mbi_h; i++)
    {
        ctx->mb_info_rows_storage[i] = mbi;
        mbi += mbi_w;
    }

    ctx->mb_info_rows = ctx->mb_info_rows_storage + 1;
}


void
vp8_dixie_modemv_destroy(struct vp8_decoder_ctx *ctx)
{
    free(ctx->mb_info_storage);
    ctx->mb_info_storage = NULL;
    free(ctx->mb_info_rows_storage);
    ctx->mb_info_rows_storage = NULL;
}
