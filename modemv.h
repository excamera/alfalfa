/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODEMV_H
#define MODEMV_H

void
vp8_dixie_modemv_init(struct vp8_decoder_ctx *ctx);


void
vp8_dixie_modemv_destroy(struct vp8_decoder_ctx *ctx);


void
vp8_dixie_modemv_process_row(struct vp8_decoder_ctx *ctx,
                             struct bool_decoder    *bool,
                             int                     row,
                             int                     start_col,
                             int                     num_cols);

#endif
