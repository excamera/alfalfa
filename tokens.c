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
#include "dixie.h"
#include "tokens.h"
#include <stdlib.h>
#include <string.h>
#include <malloc.h>


enum
{
    EOB_CONTEXT_NODE,
    ZERO_CONTEXT_NODE,
    ONE_CONTEXT_NODE,
    LOW_VAL_CONTEXT_NODE,
    TWO_CONTEXT_NODE,
    THREE_CONTEXT_NODE,
    HIGH_LOW_CONTEXT_NODE,
    CAT_ONE_CONTEXT_NODE,
    CAT_THREEFOUR_CONTEXT_NODE,
    CAT_THREE_CONTEXT_NODE,
    CAT_FIVE_CONTEXT_NODE
};
enum
{
    ZERO_TOKEN,
    ONE_TOKEN,
    TWO_TOKEN,
    THREE_TOKEN,
    FOUR_TOKEN,
    DCT_VAL_CATEGORY1,
    DCT_VAL_CATEGORY2,
    DCT_VAL_CATEGORY3,
    DCT_VAL_CATEGORY4,
    DCT_VAL_CATEGORY5,
    DCT_VAL_CATEGORY6,
    DCT_EOB_TOKEN,
    MAX_ENTROPY_TOKENS
};
struct extrabits
{
    short         min_val;
    short         length;
    unsigned char probs[12];
};
static const unsigned int left_context_index[25] =
{
    0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
    4, 4, 5, 5, 6, 6, 7, 7, 8
};
static const unsigned int above_context_index[25] =
{
    0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3,
    4, 5, 4, 5, 6, 7, 6, 7, 8
};
#define X(n) ((n) * PREV_COEF_CONTEXTS * ENTROPY_NODES)
static const unsigned int bands_x[16] =
{
    X(0), X(1), X(2), X(3), X(6), X(4), X(5), X(6),
    X(6), X(6), X(6), X(6), X(6), X(6), X(6), X(7)
};
#undef X
static const struct extrabits extrabits[MAX_ENTROPY_TOKENS] =
{
    {  0, -1, {   0,   0,   0,   0,   0,   0,
                  0,   0,   0,   0,   0,   0   } }, //ZERO_TOKEN
    {  1, 0,  {   0,   0,   0,   0,   0,   0,
                  0,   0,   0,   0,   0,   0   } }, //ONE_TOKEN
    {  2, 0,  {   0,   0,   0,   0,   0,   0,
                  0,   0,   0,   0,   0,   0   } }, //TWO_TOKEN
    {  3, 0,  {   0,   0,   0,   0,   0,   0,
                  0,   0,   0,   0,   0,   0   } }, //THREE_TOKEN
    {  4, 0,  {   0,   0,   0,   0,   0,   0,
                  0,   0,   0,   0,   0,   0   } }, //FOUR_TOKEN
    {  5, 0,  { 159,   0,   0,   0,   0,   0,
                  0,   0,   0,   0,   0,   0   } }, //DCT_VAL_CATEGORY1
    {  7, 1,  { 145, 165,   0,   0,   0,   0,
                  0,   0,   0,   0,   0,   0   } }, //DCT_VAL_CATEGORY2
    { 11, 2,  { 140, 148, 173,   0,   0,   0,
                  0,   0,   0,   0,   0,   0   } }, //DCT_VAL_CATEGORY3
    { 19, 3,  { 135, 140, 155, 176,   0,   0,
                  0,   0,   0,   0,   0,   0   } }, //DCT_VAL_CATEGORY4
    { 35, 4,  { 130, 134, 141, 157, 180,   0,
                  0,   0,   0,   0,   0,   0   } }, //DCT_VAL_CATEGORY5
    { 67, 10, { 129, 130, 133, 140, 153, 177,
                196, 230, 243, 254, 254,   0   } }, //DCT_VAL_CATEGORY6
    {  0, -1, {   0,   0,   0,   0,   0,   0,
                  0,   0,   0,   0,   0,   0   } }, // EOB TOKEN
};
static const unsigned int zigzag[16] =
{
    0,  1,  4,  8,  5,  2,  3,  6,  9, 12, 13, 10,  7, 11, 14, 15
};

#define DECODE_AND_APPLYSIGN(value_to_sign) \
    v = (bool_get_bit(boolean_decoder) ? -value_to_sign \
                            : value_to_sign) * dqf[!!c];

#define DECODE_AND_BRANCH_IF_ZERO(probability,branch) \
    if (!bool_get(boolean_decoder, probability)) goto branch;

#define DECODE_AND_LOOP_IF_ZERO(probability,branch) \
    if (!bool_get(boolean_decoder, probability)) \
    { \
        prob = type_probs; \
        if(c<15) {\
            ++c; \
            prob += bands_x[c]; \
            goto branch; \
        }\
        else \
            goto BLOCK_FINISHED; /*for malformed input */\
    }

#define DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val) \
    DECODE_AND_APPLYSIGN(val) \
    prob = type_probs + (ENTROPY_NODES*2); \
    if(c < 15){\
        b_tokens[zigzag[c]] = v; \
        ++c; \
        goto DO_WHILE; }\
    b_tokens[zigzag[15]] = v; \
    goto BLOCK_FINISHED;


#define DECODE_EXTRABIT_AND_ADJUST_VAL(t,bits_count)\
    val += bool_get(boolean_decoder, extrabits[t].probs[bits_count]) << bits_count;


static int
decode_mb_tokens(struct bool_decoder  *boolean_decoder,
                 token_entropy_ctx_t   left,
                 token_entropy_ctx_t   above,
                 short                *tokens,
                 enum prediction_mode  mode,
                 coeff_probs_table_t   probs,
                 short                 factor[TOKEN_BLOCK_TYPES][2])
{
    int            i, stop, type;
    int            c, t, v;
    int            val, bits_count;
    int            eob_mask;
    short         *b_tokens;   /* tokens for this block */
    unsigned char *type_probs; /* probabilities for this block type */
    unsigned char *prob;
    short         *dqf;

    eob_mask = 0;

    if (mode != B_PRED && mode != SPLITMV)
    {
        i = 24;
        stop = 24;
        type = 1;
        b_tokens = tokens + 24 * 16;
        dqf = factor[TOKEN_BLOCK_Y2];
    }
    else
    {
        i = 0;
        stop = 16;
        type = 3;
        b_tokens = tokens;
        dqf = factor[TOKEN_BLOCK_Y1];
    }

    /* Save a pointer to the coefficient probs for the current type.
     * Need to repeat this whenever type changes.
     */
    type_probs = probs[type][0][0];

BLOCK_LOOP:
    t = left[left_context_index[i]] + above[above_context_index[i]];
    c = !type; /* all blocks start at 0 except type 0, which starts
                * at 1. */

    prob = type_probs;
    prob += t * ENTROPY_NODES;

DO_WHILE:
    prob += bands_x[c];
    DECODE_AND_BRANCH_IF_ZERO(prob[EOB_CONTEXT_NODE], BLOCK_FINISHED);

CHECK_0_:
    DECODE_AND_LOOP_IF_ZERO(prob[ZERO_CONTEXT_NODE], CHECK_0_);
    DECODE_AND_BRANCH_IF_ZERO(prob[ONE_CONTEXT_NODE],
                              ONE_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(prob[LOW_VAL_CONTEXT_NODE],
                              LOW_VAL_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(prob[HIGH_LOW_CONTEXT_NODE],
                              HIGH_LOW_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(prob[CAT_THREEFOUR_CONTEXT_NODE],
                              CAT_THREEFOUR_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(prob[CAT_FIVE_CONTEXT_NODE],
                              CAT_FIVE_CONTEXT_NODE_0_);
    val = extrabits[DCT_VAL_CATEGORY6].min_val;
    bits_count = extrabits[DCT_VAL_CATEGORY6].length;

    do
    {
        DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY6, bits_count);
        bits_count -- ;
    }
    while (bits_count >= 0);

    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

CAT_FIVE_CONTEXT_NODE_0_:
    val = extrabits[DCT_VAL_CATEGORY5].min_val;
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY5, 4);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY5, 3);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY5, 2);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY5, 1);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY5, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

CAT_THREEFOUR_CONTEXT_NODE_0_:
    DECODE_AND_BRANCH_IF_ZERO(prob[CAT_THREE_CONTEXT_NODE],
                              CAT_THREE_CONTEXT_NODE_0_);
    val = extrabits[DCT_VAL_CATEGORY4].min_val;
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY4, 3);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY4, 2);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY4, 1);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY4, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

CAT_THREE_CONTEXT_NODE_0_:
    val = extrabits[DCT_VAL_CATEGORY3].min_val;
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY3, 2);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY3, 1);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY3, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

HIGH_LOW_CONTEXT_NODE_0_:
    DECODE_AND_BRANCH_IF_ZERO(prob[CAT_ONE_CONTEXT_NODE],
                              CAT_ONE_CONTEXT_NODE_0_);

    val = extrabits[DCT_VAL_CATEGORY2].min_val;
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY2, 1);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY2, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

CAT_ONE_CONTEXT_NODE_0_:
    val = extrabits[DCT_VAL_CATEGORY1].min_val;
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY1, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

LOW_VAL_CONTEXT_NODE_0_:
    DECODE_AND_BRANCH_IF_ZERO(prob[TWO_CONTEXT_NODE],
                              TWO_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(prob[THREE_CONTEXT_NODE],
                              THREE_CONTEXT_NODE_0_);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(4);

THREE_CONTEXT_NODE_0_:
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(3);

TWO_CONTEXT_NODE_0_:
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(2);

ONE_CONTEXT_NODE_0_:
    DECODE_AND_APPLYSIGN(1);
    prob = type_probs + ENTROPY_NODES;

    if (c < 15)
    {
        b_tokens[zigzag[c]] = v;
        ++c;
        goto DO_WHILE;
    }

    b_tokens[zigzag[15]] = v;
BLOCK_FINISHED:
    eob_mask |= (c > 1) << i;
    t = (c != !type);   // any nonzero data?
    eob_mask |= t << 31;

    left[left_context_index[i]] = above[above_context_index[i]] = t;
    b_tokens += 16;

    i++;

    if (i < stop)
        goto BLOCK_LOOP;

    if (i == 25)
    {
        type = 0;
        i = 0;
        stop = 16;
        type_probs = probs[type][0][0];
        b_tokens = tokens;
        dqf = factor[TOKEN_BLOCK_Y1];
        goto BLOCK_LOOP;
    }

    if (i == 16)
    {
        type = 2;
        type_probs = probs[type][0][0];
        stop = 24;
        dqf = factor[TOKEN_BLOCK_UV];
        goto BLOCK_LOOP;
    }

    return eob_mask;
}


static void
reset_row_context(token_entropy_ctx_t *left)
{
    memset(left, 0, sizeof(*left));
}


static void
reset_above_context(token_entropy_ctx_t *above, unsigned int cols)
{
    memset(above, 0, cols * sizeof(*above));
}


static void
reset_mb_context(token_entropy_ctx_t  *left,
                 token_entropy_ctx_t  *above,
                 enum prediction_mode  mode)
{
    /* Reset the macroblock context on the left and right. We have to
     * preserve the context of the second order block if this mode
     * would not have updated it.
     */
    memset(left, 0, sizeof((*left)[0]) * 8);
    memset(above, 0, sizeof((*above)[0]) * 8);

    if (mode != B_PRED && mode != SPLITMV)
    {
        (*left)[8] = 0;
        (*above)[8] = 0;
    }
}


void
vp8_dixie_tokens_process_row(struct vp8_decoder_ctx *ctx,
                             unsigned int            partition,
                             unsigned int            row,
                             unsigned int            start_col,
                             unsigned int            num_cols)
{
    struct token_decoder *tokens = &ctx->tokens[partition];
    short                *coeffs = tokens->coeffs + 25 * 16 * start_col;
    unsigned int          col;
    token_entropy_ctx_t  *above = ctx->above_token_entropy_ctx
                                  + start_col;
    token_entropy_ctx_t  *left = &tokens->left_token_entropy_ctx;
    struct mb_info       *mbi = ctx->mb_info_rows[row] + start_col;

    if (row == 0)
        reset_above_context(above, num_cols);

    if (start_col == 0)
        reset_row_context(left);

    for (col = start_col; col < start_col + num_cols; col++)
    {
        memset(coeffs, 0, 25 * 16 * sizeof(short));

        if (mbi->base.skip_coeff)
        {
            reset_mb_context(left, above, mbi->base.y_mode);
            mbi->base.eob_mask = 0;
        }
        else
        {
            struct dequant_factors *dqf;

            dqf = ctx->dequant_factors  + mbi->base.segment_id;
            mbi->base.eob_mask =
                decode_mb_tokens(&tokens->boolean_dec,
                                 *left, *above,
                                 coeffs,
                                 mbi->base.y_mode,
                                 ctx->entropy_hdr.coeff_probs,
                                 dqf->factor);
        }

        above++;
        mbi++;
        coeffs += 25 * 16;
    }
}


void
vp8_dixie_tokens_init(struct vp8_decoder_ctx *ctx)
{
    unsigned int  partitions = ctx->token_hdr.partitions;

    if (ctx->frame_hdr.frame_size_updated)
    {
        unsigned int i;
        unsigned int coeff_row_sz =
            ctx->mb_cols * 25 * 16 * sizeof(short);

        for (i = 0; i < partitions; i++)
        {
            free(ctx->tokens[i].coeffs);
            ctx->tokens[i].coeffs = memalign(16, coeff_row_sz);

            if (!ctx->tokens[i].coeffs)
                vpx_internal_error(&ctx->error, VPX_CODEC_MEM_ERROR,
                                   NULL);
        }

        free(ctx->above_token_entropy_ctx);
        ctx->above_token_entropy_ctx =
            calloc(ctx->mb_cols, sizeof(*ctx->above_token_entropy_ctx));

        if (!ctx->above_token_entropy_ctx)
            vpx_internal_error(&ctx->error, VPX_CODEC_MEM_ERROR, NULL);
    }
}


void
vp8_dixie_tokens_destroy(struct vp8_decoder_ctx *ctx)
{
    int i;

    for (i = 0; i < MAX_PARTITIONS; i++)
        free(ctx->tokens[i].coeffs);

    free(ctx->above_token_entropy_ctx);
}
