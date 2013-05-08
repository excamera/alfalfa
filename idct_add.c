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
#include "idct_add.h"
#include <assert.h>

void
vp8_dixie_walsh(const short *input, short *output)
{
    int i;
    int a1, b1, c1, d1;
    int a2, b2, c2, d2;
    const short *ip = input;
    short *op = output;

    for (i = 0; i < 4; i++)
    {
        a1 = ip[0] + ip[12];
        b1 = ip[4] + ip[8];
        c1 = ip[4] - ip[8];
        d1 = ip[0] - ip[12];

        op[0] = a1 + b1;
        op[4] = c1 + d1;
        op[8] = a1 - b1;
        op[12] = d1 - c1;
        ip++;
        op++;
    }

    ip = output;
    op = output;

    for (i = 0; i < 4; i++)
    {
        a1 = ip[0] + ip[3];
        b1 = ip[1] + ip[2];
        c1 = ip[1] - ip[2];
        d1 = ip[0] - ip[3];

        a2 = a1 + b1;
        b2 = c1 + d1;
        c2 = a1 - b1;
        d2 = d1 - c1;

        op[0] = (a2 + 3) >> 3;
        op[1] = (b2 + 3) >> 3;
        op[2] = (c2 + 3) >> 3;
        op[3] = (d2 + 3) >> 3;

        ip += 4;
        op += 4;
    }
}


#define cospi8sqrt2minus1 20091
#define sinpi8sqrt2       35468
#define rounding          0
static void
idct_columns(const short *input, short *output)
{
    int i;
    int a1, b1, c1, d1;

    const short *ip = input;
    short *op = output;
    int temp1, temp2;
    int shortpitch = 4;

    for (i = 0; i < 4; i++)
    {
        a1 = ip[0] + ip[8];
        b1 = ip[0] - ip[8];

        temp1 = (ip[4] * sinpi8sqrt2 + rounding) >> 16;
        temp2 = ip[12] +
            ((ip[12] * cospi8sqrt2minus1 + rounding) >> 16);
        c1 = temp1 - temp2;

        temp1 = ip[4] +
            ((ip[4] * cospi8sqrt2minus1 + rounding) >> 16);
        temp2 = (ip[12] * sinpi8sqrt2 + rounding) >> 16;
        d1 = temp1 + temp2;

        op[shortpitch*0] = a1 + d1;
        op[shortpitch*3] = a1 - d1;

        op[shortpitch*1] = b1 + c1;
        op[shortpitch*2] = b1 - c1;

        ip++;
        op++;
    }
}


void
vp8_dixie_idct_add(unsigned char        *recon,
                   const unsigned char  *predict,
                   int                   stride,
                   const short          *coeffs)
{
    int i;
    int a1, b1, c1, d1, temp1, temp2;
    short tmp[16];

    idct_columns(coeffs, tmp);
    coeffs = tmp;

    for (i = 0; i < 4; i++)
    {
        a1 = coeffs[0] + coeffs[2];
        b1 = coeffs[0] - coeffs[2];

        temp1 = (coeffs[1] * sinpi8sqrt2 + rounding) >> 16;
        temp2 = coeffs[3] +
            ((coeffs[3] * cospi8sqrt2minus1 + rounding) >> 16);
        c1 = temp1 - temp2;

        temp1 = coeffs[1] +
            ((coeffs[1] * cospi8sqrt2minus1 + rounding) >> 16);
        temp2 = (coeffs[3] * sinpi8sqrt2 + rounding) >> 16;
        d1 = temp1 + temp2;

        recon[0] = CLAMP_255(predict[0] + ((a1 + d1 + 4) >> 3));
        recon[3] = CLAMP_255(predict[3] + ((a1 - d1 + 4) >> 3));
        recon[1] = CLAMP_255(predict[1] + ((b1 + c1 + 4) >> 3));
        recon[2] = CLAMP_255(predict[2] + ((b1 - c1 + 4) >> 3));

        coeffs += 4;
        recon += stride;
        predict += stride;
    }
}
