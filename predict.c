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
#include "predict.h"
#include "idct_add.h"
#include "./mem.h"
#include <assert.h>
#include <string.h>

enum
{
    BORDER_PIXELS     = 16,
};


static const filter_t sixtap_filters[8] =
{

    { 0,  0,  128,    0,   0,  0 },
    { 0, -6,  123,   12,  -1,  0 },
    { 2, -11, 108,   36,  -8,  1 },
    { 0, -9,   93,   50,  -6,  0 },
    { 3, -16,  77,   77, -16,  3 },
    { 0, -6,   50,   93,  -9,  0 },
    { 1, -8,   36,  108, -11,  2 },
    { 0, -1,   12,  123,  -6,  0 },
};


static const filter_t bilinear_filters[8] =
{

    { 0,  0,  128,    0,   0,  0 },
    { 0,  0,  112,   16,   0,  0 },
    { 0,  0,   96,   32,   0,  0 },
    { 0,  0,   80,   48,   0,  0 },
    { 0,  0,   64,   64,   0,  0 },
    { 0,  0,   48,   80,   0,  0 },
    { 0,  0,   32,   96,   0,  0 },
    { 0,  0,   16,  112,   0,  0 }
};


static void
predict_h_nxn(unsigned char *predict,
              int            stride,
              int            n)
{
    unsigned char *left = predict - 1;
    int            i, j;

    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            predict[i *stride + j] = left[i * stride];
}


static void
predict_v_nxn(unsigned char *predict,
              int            stride,
              int            n)
{
    unsigned char *above = predict - stride;
    int            i, j;

    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            predict[i *stride + j] = above[j];
}


static void
predict_tm_nxn(unsigned char *predict,
               int            stride,
               int            n)
{
    /* Transposes the left column to the top row for later consumption
     * by the idct/recon stage
     */
    unsigned char *left = predict - 1;
    unsigned char *above = predict - stride;
    unsigned char  p = above[-1];
    int            i, j;

    for (j = 0; j < n; j++)
    {
        for (i = 0; i < n; i++)
            predict[i] = CLAMP_255(*left + above[i] - p);

        predict += stride;
        left += stride;
    }
}

static void
predict_dc_nxn(unsigned char *predict,
               int            stride,
               int            n)
{
    unsigned char *left = predict - 1;
    unsigned char *above = predict - stride;
    int            i, j, dc = 0;

    for (i = 0; i < n; i++)
    {
        dc += *left + above[i];
        left += stride;
    }

    switch (n)
    {
    case 16:
        dc = (dc + 16) >> 5;
        break;
    case  8:
        dc = (dc + 8) >> 4;
        break;
    case  4:
        dc = (dc + 4) >> 3;
        break;
    }

    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            predict[i *stride + j] = dc;
}


static void
predict_ve_4x4(unsigned char *predict,
               int            stride)
{
    unsigned char *above = predict - stride;
    int            i, j;

    predict[0] = (above[-1] + 2 * above[0] + above[1] + 2) >> 2;
    predict[1] = (above[ 0] + 2 * above[1] + above[2] + 2) >> 2;
    predict[2] = (above[ 1] + 2 * above[2] + above[3] + 2) >> 2;
    predict[3] = (above[ 2] + 2 * above[3] + above[4] + 2) >> 2;

    for (i = 1; i < 4; i++)
        for (j = 0; j < 4; j++)
            predict[i *stride + j] = predict[j];
}


static void
predict_he_4x4(unsigned char *predict,
               int            stride)
{
    unsigned char *left = predict - 1;

    predict[0] =
    predict[1] =
    predict[2] =
    predict[3] = (left[-stride] + 2 * left[0] + left[stride] + 2) >> 2;
    predict += stride;
    left += stride;

    predict[0] =
    predict[1] =
    predict[2] =
    predict[3] = (left[-stride] + 2 * left[0] + left[stride] + 2) >> 2;
    predict += stride;
    left += stride;

    predict[0] =
    predict[1] =
    predict[2] =
    predict[3] = (left[-stride] + 2 * left[0] + left[stride] + 2) >> 2;
    predict += stride;
    left += stride;

    predict[0] =
    predict[1] =
    predict[2] =
    predict[3] = (left[-stride] + 2 * left[0] + left[0] + 2) >> 2;
}


static void
predict_ld_4x4(unsigned char *predict,
               int            stride)
{
    unsigned char *above = predict - stride;
    int            pred0, pred1, pred2, pred3, pred4, pred5, pred6;

    predict[0] = pred0 = (above[0] + 2 * above[1] + above[2] + 2) >> 2;
    predict[1] = pred1 = (above[1] + 2 * above[2] + above[3] + 2) >> 2;
    predict[2] = pred2 = (above[2] + 2 * above[3] + above[4] + 2) >> 2;
    predict[3] = pred3 = (above[3] + 2 * above[4] + above[5] + 2) >> 2;
    predict += stride;

    predict[0] = pred1;
    predict[1] = pred2;
    predict[2] = pred3;
    predict[3] = pred4 = (above[4] + 2 * above[5] + above[6] + 2) >> 2;
    predict += stride;

    predict[0] = pred2;
    predict[1] = pred3;
    predict[2] = pred4;
    predict[3] = pred5 = (above[5] + 2 * above[6] + above[7] + 2) >> 2;
    predict += stride;

    predict[0] = pred3;
    predict[1] = pred4;
    predict[2] = pred5;
    predict[3] = pred6 = (above[6] + 2 * above[7] + above[7] + 2) >> 2;
}


static void
predict_rd_4x4(unsigned char *predict,
               int            stride)
{
    unsigned char *left = predict - 1;
    unsigned char *above = predict - stride;
    int            pred0, pred1, pred2, pred3, pred4, pred5, pred6;

    predict[0] = pred0 =
        (left[ 0] + 2 * above[-1] + above[0] + 2) >> 2;
    predict[1] = pred1 =
        (above[-1] + 2 * above[ 0] + above[1] + 2) >> 2;
    predict[2] = pred2 =
        (above[ 0] + 2 * above[ 1] + above[2] + 2) >> 2;
    predict[3] = pred3 =
        (above[ 1] + 2 * above[ 2] + above[3] + 2) >> 2;
    predict += stride;

    predict[0] = pred4 =
        (left[stride] + 2 * left[0] + above[-1] + 2) >> 2;
    predict[1] = pred0;
    predict[2] = pred1;
    predict[3] = pred2;
    predict += stride;

    predict[0] = pred5 =
        (left[stride*2] + 2 * left[stride] + left[0] + 2) >> 2;
    predict[1] = pred4;
    predict[2] = pred0;
    predict[3] = pred1;
    predict += stride;

    predict[0] = pred6 =
        (left[stride*3] + 2 * left[stride*2] + left[stride] + 2) >> 2;
    predict[1] = pred5;
    predict[2] = pred4;
    predict[3] = pred0;
}


static void
predict_vr_4x4(unsigned char *predict,
               int            stride)
{
    unsigned char *left = predict - 1;
    unsigned char *above = predict - stride;
    int            pred0, pred1, pred2, pred3, pred4, pred5, pred6,
                   pred7, pred8, pred9;

    predict[0] = pred0 = (above[-1] + above[0] + 1) >> 1;
    predict[1] = pred1 = (above[ 0] + above[1] + 1) >> 1;
    predict[2] = pred2 = (above[ 1] + above[2] + 1) >> 1;
    predict[3] = pred3 = (above[ 2] + above[3] + 1) >> 1;
    predict += stride;

    predict[0] = pred4 =
        (left[ 0] + 2 * above[-1] + above[0] + 2) >> 2;
    predict[1] = pred5 =
        (above[-1] + 2 * above[ 0] + above[1] + 2) >> 2;
    predict[2] = pred6 =
        (above[ 0] + 2 * above[ 1] + above[2] + 2) >> 2;
    predict[3] = pred7 =
        (above[ 1] + 2 * above[ 2] + above[3] + 2) >> 2;
    predict += stride;

    predict[0] = pred8 =
        (left[stride] + 2 * left[0] + above[-1] + 2) >> 2;
    predict[1] = pred0;
    predict[2] = pred1;
    predict[3] = pred2;
    predict += stride;

    predict[0] = pred9 =
        (left[stride*2] + 2 * left[stride] + left[0] + 2) >> 2;
    predict[1] = pred4;
    predict[2] = pred5;
    predict[3] = pred6;
}


static void
predict_vl_4x4(unsigned char *predict,
               int            stride)
{
    unsigned char *above = predict - stride;
    int            pred0, pred1, pred2, pred3, pred4, pred5, pred6,
                   pred7, pred8, pred9;

    predict[0] = pred0 = (above[0] + above[1] + 1) >> 1;
    predict[1] = pred1 = (above[1] + above[2] + 1) >> 1;
    predict[2] = pred2 = (above[2] + above[3] + 1) >> 1;
    predict[3] = pred3 = (above[3] + above[4] + 1) >> 1;
    predict += stride;

    predict[0] = pred4 =
        (above[0] + 2 * above[1] + above[2] + 2) >> 2;
    predict[1] = pred5 =
        (above[1] + 2 * above[2] + above[3] + 2) >> 2;
    predict[2] = pred6 =
        (above[2] + 2 * above[3] + above[4] + 2) >> 2;
    predict[3] = pred7 =
        (above[3] + 2 * above[4] + above[5] + 2) >> 2;
    predict += stride;

    predict[0] = pred1;
    predict[1] = pred2;
    predict[2] = pred3;
    predict[3] = pred8 = (above[4] + 2 * above[5] + above[6] + 2) >> 2;
    predict += stride;

    predict[0] = pred5;
    predict[1] = pred6;
    predict[2] = pred7;
    predict[3] = pred9 = (above[5] + 2 * above[6] + above[7] + 2) >> 2;
}


static void
predict_hd_4x4(unsigned char *predict,
               int            stride)
{
    unsigned char *left = predict - 1;
    unsigned char *above = predict - stride;
    int            pred0, pred1, pred2, pred3, pred4, pred5, pred6,
                   pred7, pred8, pred9;

    predict[0] = pred0 =
        (left[ 0] + above[-1] + 1) >> 1;
    predict[1] = pred1 =
        (left[ 0] + 2 * above[-1] + above[0] + 2) >> 2;
    predict[2] = pred2 =
        (above[-1] + 2 * above[ 0] + above[1] + 2) >> 2;
    predict[3] = pred3 =
        (above[ 0] + 2 * above[ 1] + above[2] + 2) >> 2;
    predict += stride;

    predict[0] = pred4 =
        (left[stride] + left[0] + 1) >> 1;
    predict[1] = pred5 =
        (left[stride] + 2 * left[0] + above[-1] + 2) >> 2;
    predict[2] = pred0;
    predict[3] = pred1;
    predict += stride;

    predict[0] = pred6 =
        (left[stride*2] +   left[stride] + 1) >> 1;
    predict[1] = pred7 =
        (left[stride*2] + 2 * left[stride] + left[0] + 2) >> 2;
    predict[2] = pred4;
    predict[3] = pred5;
    predict += stride;

    predict[0] = pred8 =
        (left[stride*3] +   left[stride*2] + 1) >> 1;
    predict[1] = pred9 =
        (left[stride*3] + 2 * left[stride*2] + left[stride] + 2) >> 2;
    predict[2] = pred6;
    predict[3] = pred7;
}


static void
predict_hu_4x4(unsigned char *predict,
               int            stride)
{
    unsigned char *left = predict - 1;
    int            pred0, pred1, pred2, pred3, pred4, pred5, pred6;

    predict[0] = pred0 =
        (left[stride*0] +   left[stride*1] + 1) >> 1;
    predict[1] = pred1 =
        (left[stride*0] + 2 * left[stride*1] + left[stride*2] + 2) >> 2;
    predict[2] = pred2 =
        (left[stride*1] +   left[stride*2] + 1) >> 1;
    predict[3] = pred3 =
        (left[stride*1] + 2 * left[stride*2] + left[stride*3] + 2) >> 2;
    predict += stride;

    predict[0] = pred2;
    predict[1] = pred3;
    predict[2] = pred4 =
        (left[stride*2] + left[stride*3] + 1) >> 1;
    predict[3] = pred5 =
        (left[stride*2] + 2 * left[stride*3] + left[stride*3] + 2) >> 2;
    predict += stride;

    predict[0] = pred4;
    predict[1] = pred5;
    predict[2] = pred6 = left[stride*3];
    predict[3] = pred6;
    predict += stride;

    predict[0] = pred6;
    predict[1] = pred6;
    predict[2] = pred6;
    predict[3] = pred6;
}


static void
predict_h_16x16(unsigned char *predict, int stride)
{
    predict_h_nxn(predict, stride, 16);
}


static void
predict_v_16x16(unsigned char *predict, int stride)
{
    predict_v_nxn(predict, stride, 16);
}


static void
predict_tm_16x16(unsigned char *predict, int stride)
{
    predict_tm_nxn(predict, stride, 16);
}


static void
predict_h_8x8(unsigned char *predict, int stride)
{
    predict_h_nxn(predict, stride, 8);
}


static void
predict_v_8x8(unsigned char *predict, int stride)
{
    predict_v_nxn(predict, stride, 8);
}


static void
predict_tm_8x8(unsigned char *predict, int stride)
{
    predict_tm_nxn(predict, stride, 8);
}


static void
predict_tm_4x4(unsigned char *predict, int stride)
{
    predict_tm_nxn(predict, stride, 4);
}


static void
copy_down(unsigned char           *recon,
          int                      stride)
{
    /* Copy the four pixels above-right of subblock 3 to
     * above-right of subblocks 7, 11, and 15
     */
    uint32_t tmp, *copy = (void *)(recon + 16 - stride);

    stride = stride / sizeof(unsigned int);
    tmp = *copy;
    copy += stride * 4;
    *copy = tmp;
    copy += stride * 4;
    *copy = tmp;
    copy += stride * 4;
    *copy = tmp;
}


static void
b_pred(unsigned char  *predict,
       int             stride,
       struct mb_info *mbi,
       short          *coeffs)
{
    int i;

    copy_down(predict, stride);

    for (i = 0; i < 16; i++)
    {
        unsigned char *b_predict = predict + (i & 3) * 4;

        switch (mbi->split.modes[i])
        {
        case B_DC_PRED:
            predict_dc_nxn(b_predict, stride, 4);
            break;
        case B_TM_PRED:
            predict_tm_4x4(b_predict, stride);
            break;
        case B_VE_PRED:
            predict_ve_4x4(b_predict, stride);
            break;
        case B_HE_PRED:
            predict_he_4x4(b_predict, stride);
            break;
        case B_LD_PRED:
            predict_ld_4x4(b_predict, stride);
            break;
        case B_RD_PRED:
            predict_rd_4x4(b_predict, stride);
            break;
        case B_VR_PRED:
            predict_vr_4x4(b_predict, stride);
            break;
        case B_VL_PRED:
            predict_vl_4x4(b_predict, stride);
            break;
        case B_HD_PRED:
            predict_hd_4x4(b_predict, stride);
            break;
        case B_HU_PRED:
            predict_hu_4x4(b_predict, stride);
            break;
        default:
            assert(0);
        }

        vp8_dixie_idct_add(b_predict, b_predict, stride, coeffs);
        coeffs += 16;

        if ((i & 3) == 3)
        {
            predict += stride * 4;
        }
    }
}


static void
fixup_dc_coeffs(struct mb_info *mbi,
                short          *coeffs)
{
    short y2[16];
    int   i;

    vp8_dixie_walsh(coeffs + 24 * 16, y2);

    for (i = 0; i < 16; i++)
        coeffs[i*16] = y2[i];
}


static void
predict_intra_luma(unsigned char   *predict,
                   int              stride,
                   struct mb_info  *mbi,
                   short           *coeffs)
{
    if (mbi->base.y_mode == B_PRED)
        b_pred(predict, stride, mbi, coeffs);
    else
    {
        int i;

        switch (mbi->base.y_mode)
        {
        case DC_PRED:
            predict_dc_nxn(predict, stride, 16);
            break;
        case V_PRED:
            predict_v_16x16(predict, stride);
            break;
        case H_PRED:
            predict_h_16x16(predict, stride);
            break;
        case TM_PRED:
            predict_tm_16x16(predict, stride);
            break;
        default:
            assert(0);
        }

        fixup_dc_coeffs(mbi, coeffs);

        for (i = 0; i < 16; i++)
        {
            vp8_dixie_idct_add(predict, predict, stride, coeffs);
            coeffs += 16;
            predict += 4;

            if ((i & 3) == 3)
                predict += stride * 4 - 16;
        }

    }
}


static void
predict_intra_chroma(unsigned char   *predict_u,
                     unsigned char   *predict_v,
                     int              stride,
                     struct mb_info  *mbi,
                     short           *coeffs)
{
    int i;

    switch (mbi->base.uv_mode)
    {
    case DC_PRED:
        predict_dc_nxn(predict_u, stride, 8);
        predict_dc_nxn(predict_v, stride, 8);
        break;
    case V_PRED:
        predict_v_8x8(predict_u, stride);
        predict_v_8x8(predict_v, stride);
        break;
    case H_PRED:
        predict_h_8x8(predict_u, stride);
        predict_h_8x8(predict_v, stride);
        break;
    case TM_PRED:
        predict_tm_8x8(predict_u, stride);
        predict_tm_8x8(predict_v, stride);
        break;
    default:
        assert(0);
    }

    coeffs += 16 * 16;

    for (i = 16; i < 20; i++)
    {
        vp8_dixie_idct_add(predict_u, predict_u, stride, coeffs);
        coeffs += 16;
        predict_u += 4;

        if (i & 1)
            predict_u += stride * 4 - 8;
    }

    for (i = 20; i < 24; i++)
    {
        vp8_dixie_idct_add(predict_v, predict_v, stride, coeffs);
        coeffs += 16;
        predict_v += 4;

        if (i & 1)
            predict_v += stride * 4 - 8;
    }

}


static void
sixtap_horiz(unsigned char       *output,
             int                  output_stride,
             const unsigned char *reference,
             int                  reference_stride,
             int                  cols,
             int                  rows,
             const filter_t       filter
            )
{
    int r, c, temp;

    for (r = 0; r < rows; r++)
    {
        for (c = 0; c < cols; c++)
        {
            temp = (reference[-2] * filter[0]) +
                   (reference[-1] * filter[1]) +
                   (reference[ 0] * filter[2]) +
                   (reference[ 1] * filter[3]) +
                   (reference[ 2] * filter[4]) +
                   (reference[ 3] * filter[5]) +
                   64;
            temp >>= 7;
            output[c] = CLAMP_255(temp);
            reference++;
        }

        reference += reference_stride - cols;
        output += output_stride;
    }
}


static void
sixtap_vert(unsigned char       *output,
            int                  output_stride,
            const unsigned char *reference,
            int                  reference_stride,
            int                  cols,
            int                  rows,
            const filter_t       filter
           )
{
    int r, c, temp;

    for (r = 0; r < rows; r++)
    {
        for (c = 0; c < cols; c++)
        {
            temp = (reference[-2*reference_stride] * filter[0]) +
                   (reference[-1*reference_stride] * filter[1]) +
                   (reference[ 0*reference_stride] * filter[2]) +
                   (reference[ 1*reference_stride] * filter[3]) +
                   (reference[ 2*reference_stride] * filter[4]) +
                   (reference[ 3*reference_stride] * filter[5]) +
                   64;
            temp >>= 7;
            output[c] = CLAMP_255(temp);
            reference++;
        }

        reference += reference_stride - cols;
        output += output_stride;
    }
}


static void
sixtap_2d(unsigned char       *output,
          int                  output_stride,
          const unsigned char *reference,
          int                  reference_stride,
          int                  cols,
          int                  rows,
          int                  mx,
          int                  my,
          const filter_t       filters[8]
         )
{
    DECLARE_ALIGNED(16, unsigned char, temp[16*(16+5)]);

    sixtap_horiz(temp, 16,
                 reference - 2 * reference_stride, reference_stride,
                 cols, rows + 5, filters[mx]);
    sixtap_vert(output, output_stride,
                temp + 2 * 16, 16,
                cols, rows, filters[my]);
}


struct img_index
{
    unsigned char *y, *u, *v;
    int            stride, uv_stride;
};


static const unsigned char *
filter_block(unsigned char        *output,
             const unsigned char  *reference,
             int                   stride,
             const union mv       *mv,
             const filter_t        filters[8])
{
    int mx, my;

    /* Handle 0,0 as a special case. TODO: does this make it any
     * faster?
     */
    if (!mv->raw)
        return reference;

    mx = mv->d.x & 7;
    my = mv->d.y & 7;
    reference += ((mv->d.y >> 3) * stride) + (mv->d.x >> 3);

    if (mx | my)
    {
        sixtap_2d(output, stride, reference, stride, 4, 4, mx, my,
                  filters);
        reference = output;
    }

    return reference;
}


static void
recon_1_block(unsigned char        *output,
              const unsigned char  *reference,
              int                   stride,
              const union mv       *mv,
              const filter_t        filters[8],
              short                *coeffs,
              struct mb_info       *mbi,
              int                   b
             )
{
    const unsigned char *predict;

    predict = filter_block(output, reference, stride, mv, filters);
    vp8_dixie_idct_add(output, predict, stride, coeffs + 16 * b);
}


static mv_t
calculate_chroma_splitmv(struct mb_info *mbi,
                         int             b,
                         int             full_pixel)
{
    int temp;
    union mv mv;

    temp = mbi->split.mvs[b].d.x +
           mbi->split.mvs[b+1].d.x +
           mbi->split.mvs[b+4].d.x +
           mbi->split.mvs[b+5].d.x;

    if (temp < 0)
        temp -= 4;
    else
        temp += 4;

    mv.d.x = temp / 8;

    temp = mbi->split.mvs[b].d.y +
           mbi->split.mvs[b+1].d.y +
           mbi->split.mvs[b+4].d.y +
           mbi->split.mvs[b+5].d.y;

    if (temp < 0)
        temp -= 4;
    else
        temp += 4;

    mv.d.y = temp / 8;

    if (full_pixel)
    {
        mv.d.x &= ~7;
        mv.d.y &= ~7;
    }

    return mv;
}


/* Note: We rely on the reconstructed border having the same stride as
 * the reference buffer because the filter_block can't adjust the
 * stride with its return value, only the reference pointer.
 */
static void
build_mc_border(unsigned char       *dst,
                const unsigned char *src,
                int                  stride,
                int                  x,
                int                  y,
                int                  b_w,
                int                  b_h,
                int                  w,
                int                  h
               )
{
    const unsigned char *ref_row;


    /* Get a pointer to the start of the real data for this row */
    ref_row = src - x - y * stride;

    if (y >= h)
        ref_row += (h - 1) * stride;
    else if (y > 0)
        ref_row += y * stride;

    do
    {
        int left, right = 0, copy;

        left = x < 0 ? -x : 0;

        if (left > b_w)
            left = b_w;

        if (x + b_w > w)
            right = x + b_w - w;

        if (right > b_w)
            right = b_w;

        copy = b_w - left - right;

        if (left)
            memset(dst, ref_row[0], left);

        if (copy)
            memcpy(dst + left, ref_row + x + left, copy);

        if (right)
            memset(dst + left + copy, ref_row[w-1], right);

        dst += stride;
        y++;

        if (y < h && y > 0)
            ref_row += stride;
    }
    while (--b_h);
}


static void
recon_1_edge_block(unsigned char        *output,
                   unsigned char        *emul_block,
                   const unsigned char  *reference,
                   int                   stride,
                   const union mv       *mv,
                   const filter_t        filters[8],
                   short                *coeffs,
                   struct mb_info       *mbi,
                   int                   x,
                   int                   y,
                   int                   w,
                   int                   h,
                   int                   start_b
                  )
{
    const unsigned char *predict;
    int                  b = start_b;
    const int            b_w = 4;
    const int            b_h = 4;

    x += mv->d.x >> 3;
    y += mv->d.y >> 3;

    /* Need two pixels left/above, 3 right/below for 6-tap */
    if (x < 2 || x + b_w - 1 + 3 >= w || y < 2 || y + b_h - 1 + 3 >= h)
    {
        reference += (mv->d.x >> 3) + (mv->d.y >> 3) * stride;
        build_mc_border(emul_block,
                        reference - 2 - 2 * stride, stride,
                        x - 2, y - 2, b_w + 5, b_h + 5, w, h);
        reference = emul_block + 2 * stride + 2;
        reference -= (mv->d.x >> 3) + (mv->d.y >> 3) * stride;
    }

    predict = filter_block(output, reference, stride, mv, filters);
    vp8_dixie_idct_add(output, predict, stride, coeffs + 16 * b);
}


static void
predict_inter_emulated_edge(struct vp8_decoder_ctx  *ctx,
                            struct img_index        *img,
                            short                   *coeffs,
                            struct mb_info          *mbi,
                            int                      mb_col,
                            int                      mb_row)
{
    /* TODO: move this into its own buffer. This only works because we
     * still have a border allocated.
     */
    unsigned char *emul_block = ctx->frame_strg[0].img.img_data;
    unsigned char *reference;
    unsigned char *output;
    ptrdiff_t      reference_offset;
    int            w, h, x, y, b;
    union mv       chroma_mv[4];
    unsigned char *u = img->u, *v = img->v;
    int            full_pixel = ctx->frame_hdr.version == 3;


    x = mb_col * 16;
    y = mb_row * 16;
    w = ctx->mb_cols * 16;
    h = ctx->mb_rows * 16;
    output = img->y;
    reference_offset = ctx->ref_frame_offsets[mbi->base.ref_frame];
    reference = output + reference_offset;

    if (mbi->base.y_mode != SPLITMV)
    {
        union mv uvmv;

        uvmv = mbi->base.mv;
        uvmv.d.x = (uvmv.d.x + 1 + (uvmv.d.x >> 31) * 2) / 2;
        uvmv.d.y = (uvmv.d.y + 1 + (uvmv.d.y >> 31) * 2) / 2;

        if (full_pixel)
        {
            uvmv.d.x &= ~7;
            uvmv.d.y &= ~7;
        }

        chroma_mv[0] = uvmv;
        chroma_mv[1] = uvmv;
        chroma_mv[2] = uvmv;
        chroma_mv[3] = uvmv;
    }
    else
    {
        chroma_mv[0] = calculate_chroma_splitmv(mbi,  0, full_pixel);
        chroma_mv[1] = calculate_chroma_splitmv(mbi,  2, full_pixel);
        chroma_mv[2] = calculate_chroma_splitmv(mbi,  8, full_pixel);
        chroma_mv[3] = calculate_chroma_splitmv(mbi, 10, full_pixel);
    }


    /* Luma */
    for (b = 0; b < 16; b++)
    {
        union mv *ymv;

        if (mbi->base.y_mode != SPLITMV)
            ymv = &mbi->base.mv;
        else
            ymv = mbi->split.mvs + b;

        recon_1_edge_block(output, emul_block, reference, img->stride,
                           ymv, ctx->subpixel_filters,
                           coeffs, mbi, x, y, w, h, b);

        x += 4;
        output += 4;
        reference += 4;

        if ((b & 3) == 3)
        {
            x -= 16;
            y += 4;
            output += 4 * img->stride - 16;
            reference += 4 * img->stride - 16;
        }
    }

    x = mb_col * 16;
    y = mb_row * 16;

    /* Chroma */
    x >>= 1;
    y >>= 1;
    w >>= 1;
    h >>= 1;

    for (b = 0; b < 4; b++)
    {
        recon_1_edge_block(u, emul_block, u + reference_offset,
                           img->uv_stride,
                           &chroma_mv[b], ctx->subpixel_filters,
                           coeffs, mbi, x, y, w, h, b + 16);
        recon_1_edge_block(v, emul_block, v + reference_offset,
                           img->uv_stride,
                           &chroma_mv[b], ctx->subpixel_filters,
                           coeffs, mbi, x, y, w, h, b + 20);
        u += 4;
        v += 4;
        x += 4;

        if (b & 1)
        {
            x -= 8;
            y += 4;
            u += 4 * img->uv_stride - 8;
            v += 4 * img->uv_stride - 8;
        }
    }

}

static void
predict_inter(struct vp8_decoder_ctx  *ctx,
              struct img_index        *img,
              short                   *coeffs,
              struct mb_info          *mbi)
{
    unsigned char *y = img->y;
    unsigned char *u = img->u;
    unsigned char *v = img->v;
    ptrdiff_t      reference_offset;
    union mv       chroma_mv[4];
    int            full_pixel = ctx->frame_hdr.version == 3;
    int b;

    if (mbi->base.y_mode != SPLITMV)
    {
        union mv             uvmv;

        uvmv = mbi->base.mv;
        uvmv.d.x = (uvmv.d.x + 1 + (uvmv.d.x >> 31) * 2) / 2;
        uvmv.d.y = (uvmv.d.y + 1 + (uvmv.d.y >> 31) * 2) / 2;

        if (full_pixel)
        {
            uvmv.d.x &= ~7;
            uvmv.d.y &= ~7;
        }

        chroma_mv[0] =
            chroma_mv[1] =
                chroma_mv[2] =
                    chroma_mv[3] = uvmv;
    }
    else
    {
        chroma_mv[0] = calculate_chroma_splitmv(mbi,  0, full_pixel);
        chroma_mv[1] = calculate_chroma_splitmv(mbi,  2, full_pixel);
        chroma_mv[2] = calculate_chroma_splitmv(mbi,  8, full_pixel);
        chroma_mv[3] = calculate_chroma_splitmv(mbi, 10, full_pixel);
    }

    reference_offset = ctx->ref_frame_offsets[mbi->base.ref_frame];

    for (b = 0; b < 16; b++)
    {
        union mv *ymv;

        if (mbi->base.y_mode != SPLITMV)
            ymv = &mbi->base.mv;
        else
            ymv = mbi->split.mvs + b;


        recon_1_block(y, y + reference_offset, img->stride,
                      ymv, ctx->subpixel_filters, coeffs, mbi, b);
        y += 4;

        if ((b & 3) == 3)
            y += 4 * img->stride - 16;
    }

    for (b = 0; b < 4; b++)
    {
        recon_1_block(u, u + reference_offset,
                      img->uv_stride, &chroma_mv[b],
                      ctx->subpixel_filters, coeffs, mbi, b + 16);
        recon_1_block(v, v + reference_offset,
                      img->uv_stride, &chroma_mv[b],
                      ctx->subpixel_filters, coeffs, mbi, b + 20);
        u += 4;
        v += 4;

        if (b & 1)
        {
            u += 4 * img->uv_stride - 8;
            v += 4 * img->uv_stride - 8;
        }
    }
}


void
vp8_dixie_release_ref_frame(struct ref_cnt_img *rcimg)
{
    if (rcimg)
    {
        assert(rcimg->ref_cnt);
        rcimg->ref_cnt--;
    }
}


struct ref_cnt_img *
vp8_dixie_ref_frame(struct ref_cnt_img *rcimg)
{
    rcimg->ref_cnt++;
    return rcimg;
}


struct ref_cnt_img *
vp8_dixie_find_free_ref_frame(struct ref_cnt_img *frames)
{
    int i;

    for (i = 0; i < NUM_REF_FRAMES; i++)
        if (frames[i].ref_cnt == 0)
        {
            frames[i].ref_cnt = 1;
            return &frames[i];
        }

    assert(0);
    return NULL;
}


static void
fixup_left(unsigned char        *predict,
           int                   width,
           int                   stride,
           unsigned int          row,
           enum prediction_mode  mode)
{
    /* The left column of out-of-frame pixels is taken to be 129,
     * unless we're doing DC_PRED, in which case we duplicate the
     * above row, unless this is also row 0, in which case we use
     * 129.
     */
    unsigned char *left = predict - 1;
    int i;

    if (mode == DC_PRED && row)
    {
        unsigned char *above = predict - stride;

        for (i = 0; i < width; i++)
        {
            *left = above[i];
            left += stride;
        }
    }
    else
    {
        /* Need to re-set the above row, in case the above MB was
         * DC_PRED.
         */
        left -= stride;

        for (i = -1; i < width; i++)
        {
            *left = 129;
            left += stride;
        }
    }
}


static void
fixup_above(unsigned char        *predict,
            int                   width,
            int                   stride,
            unsigned int          col,
            enum prediction_mode  mode)
{
    /* The above row of out-of-frame pixels is taken to be 127,
     * unless we're doing DC_PRED, in which case we duplicate the
     * left col, unless this is also col 0, in which case we use
     * 127.
     */
    unsigned char *above = predict - stride;
    int i;

    if (mode == DC_PRED && col)
    {
        unsigned char *left = predict - 1;

        for (i = 0; i < width; i++)
        {
            above[i] = *left;
            left += stride;
        }
    }
    else
        /* Need to re-set the left col, in case the last MB was
         * DC_PRED.
         */
        memset(above - 1, 127, width + 1);

    memset(above + width, 127, 4); // for above-right subblock modes
}


void
vp8_dixie_predict_init(struct vp8_decoder_ctx *ctx)
{

    int i;
    unsigned char *this_frame_base;

    if (ctx->frame_hdr.frame_size_updated)
    {
        for (i = 0; i < NUM_REF_FRAMES; i++)
        {
            unsigned int w = ctx->mb_cols * 16 + BORDER_PIXELS * 2;
            unsigned int h = ctx->mb_rows * 16 + BORDER_PIXELS * 2;

            vpx_img_free(&ctx->frame_strg[i].img);
            ctx->frame_strg[i].ref_cnt = 0;
            ctx->ref_frames[i] = NULL;

            if (!vpx_img_alloc(&ctx->frame_strg[i].img,
                               IMG_FMT_I420, w, h, 16))
                vpx_internal_error(&ctx->error, VPX_CODEC_MEM_ERROR,
                                   "Failed to allocate %dx%d"
                                   " framebuffer",
                                   w, h);

            vpx_img_set_rect(&ctx->frame_strg[i].img,
                             BORDER_PIXELS, BORDER_PIXELS,
                             ctx->frame_hdr.kf.w, ctx->frame_hdr.kf.h);

        }

        if (ctx->frame_hdr.version)
            ctx->subpixel_filters = bilinear_filters;
        else
            ctx->subpixel_filters = sixtap_filters;
    }

    /* Find a free framebuffer to predict into */
    if (ctx->ref_frames[CURRENT_FRAME])
        vp8_dixie_release_ref_frame(ctx->ref_frames[CURRENT_FRAME]);

    ctx->ref_frames[CURRENT_FRAME] =
        vp8_dixie_find_free_ref_frame(ctx->frame_strg);
    this_frame_base = ctx->ref_frames[CURRENT_FRAME]->img.img_data;

    /* Calculate offsets to the other reference frames */
    for (i = 0; i < NUM_REF_FRAMES; i++)
    {
        struct ref_cnt_img  *ref = ctx->ref_frames[i];

        ctx->ref_frame_offsets[i] =
            ref ? ref->img.img_data - this_frame_base : 0;
    }

    /* TODO: No need to do this on every frame... */
}


void
vp8_dixie_predict_destroy(struct vp8_decoder_ctx *ctx)
{
    int i;

    for (i = 0; i < NUM_REF_FRAMES; i++)
    {
        vpx_img_free(&ctx->frame_strg[i].img);
        ctx->frame_strg[i].ref_cnt = 0;
        ctx->ref_frames[i] = NULL;
    }
}


void
vp8_dixie_predict_process_row(struct vp8_decoder_ctx *ctx,
                              unsigned int            row,
                              unsigned int            start_col,
                              unsigned int            num_cols)
{
    struct img_index img;
    struct mb_info *mbi;
    unsigned int    col;
    short          *coeffs;

    /* Adjust pointers based on row, start_col */
    img.stride    = ctx->ref_frames[CURRENT_FRAME]->img.stride[PLANE_Y];
    img.uv_stride = ctx->ref_frames[CURRENT_FRAME]->img.stride[PLANE_U];
    img.y = ctx->ref_frames[CURRENT_FRAME]->img.planes[PLANE_Y];
    img.u = ctx->ref_frames[CURRENT_FRAME]->img.planes[PLANE_U];
    img.v = ctx->ref_frames[CURRENT_FRAME]->img.planes[PLANE_V];
    img.y += (img.stride * row + start_col) * 16;
    img.u += (img.uv_stride * row + start_col) * 8;
    img.v += (img.uv_stride * row + start_col) * 8;
    mbi = ctx->mb_info_rows[row] + start_col;
    coeffs = ctx->tokens[row & (ctx->token_hdr.partitions - 1)].coeffs
             + 25 * 16 * start_col;

    /* Fix up the out-of-frame pixels */
    if (start_col == 0)
    {
        fixup_left(img.y, 16, img.stride, row, mbi->base.y_mode);
        fixup_left(img.u, 8, img.uv_stride, row, mbi->base.uv_mode);
        fixup_left(img.v, 8, img.uv_stride, row, mbi->base.uv_mode);

        if (row == 0)
            *(img.y - img.stride - 1) = 127;
    }

    for (col = start_col; col < start_col + num_cols; col++)
    {
        if (row == 0)
        {
            fixup_above(img.y, 16, img.stride, col, mbi->base.y_mode);
            fixup_above(img.u, 8, img.uv_stride, col,
                        mbi->base.uv_mode);
            fixup_above(img.v, 8, img.uv_stride, col,
                        mbi->base.uv_mode);
        }

        if (mbi->base.y_mode <= B_PRED)
        {
            predict_intra_luma(img.y, img.stride, mbi, coeffs);
            predict_intra_chroma(img.u, img.v, img.uv_stride, mbi,
                                 coeffs);
        }
        else
        {
            if (mbi->base.y_mode != SPLITMV) // && != BPRED
                fixup_dc_coeffs(mbi, coeffs);

            if (mbi->base.need_mc_border)
                predict_inter_emulated_edge(ctx, &img, coeffs, mbi, col,
                                            row);
            else
                predict_inter(ctx, &img, coeffs, mbi);
        }

        /* Advance to the next macroblock */
        mbi++;
        img.y += 16;
        img.u += 8;
        img.v += 8;
        coeffs += 25 * 16;
    }

    if (col == ctx->mb_cols)
    {
        /* Extend the last row by four pixels for intra prediction.
         * This will be propagated later by copy_down.
         */
        uint32_t *extend = (uint32_t *)(img.y + 15 * img.stride);
        uint32_t  val = 0x01010101 * img.y[-1 + 15 * img.stride];
        *extend = val;
    }
}
