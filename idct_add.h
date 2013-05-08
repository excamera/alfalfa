/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef IDCT_ADD_H
#define IDCT_ADD_H

void
vp8_dixie_idct_add_init(struct vp8_decoder_ctx *ctx);


void
vp8_dixie_idct_add(unsigned char        *recon,
                   const unsigned char  *predict,
                   int                   stride,
                   const short          *coeffs);


void
vp8_dixie_walsh(const short *in, short *out);


void
vp8_dixie_idct_add_process_row(struct vp8_decoder_ctx *ctx,
                               short                  *coeffs,
                               unsigned int            row,
                               unsigned int            start_col,
                               unsigned int            num_cols);

#endif
