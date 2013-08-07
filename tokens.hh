/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TOKENS_H
#define TOKENS_H

void
vp8_dixie_tokens_init(struct vp8_decoder_ctx *ctx);


void
vp8_dixie_tokens_destroy(struct vp8_decoder_ctx *ctx);


void
vp8_dixie_tokens_process_row(struct vp8_decoder_ctx *ctx,
                             unsigned int            partition,
                             unsigned int            row,
                             unsigned int            start_col,
                             unsigned int            num_cols);

#endif
