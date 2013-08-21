/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef PREDICT_H
#define PREDICT_H

void
vp8_dixie_predict_init(struct vp8_decoder_ctx *ctx);


void
vp8_dixie_predict_destroy(struct vp8_decoder_ctx *ctx);


void
vp8_dixie_predict_process_row(struct vp8_decoder_ctx *ctx,
                              unsigned int            row,
                              unsigned int            start_col,
                              unsigned int            num_cols);

void
vp8_dixie_release_ref_frame(struct ref_cnt_img *rcimg);

struct ref_cnt_img *
vp8_dixie_ref_frame(struct ref_cnt_img *rcimg);

struct ref_cnt_img *
vp8_dixie_find_free_ref_frame(struct ref_cnt_img *frames);

#endif
