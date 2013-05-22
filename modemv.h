/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifdef __cplusplus
extern "C" {
#endif

#ifndef MODEMV_H
#define MODEMV_H

struct mv_clamp_rect
{
    int to_left, to_right, to_top, to_bottom;
};

void
vp8_dixie_modemv_init(struct vp8_decoder_ctx *ctx);


void
vp8_dixie_modemv_destroy(struct vp8_decoder_ctx *ctx);


void
vp8_dixie_modemv_process_row(struct vp8_decoder_ctx *ctx,
                             struct bool_decoder    *entropy_decoder,
                             int                     row,
                             int                     start_col,
                             int                     num_cols);

int read_segment_id(struct bool_decoder* entropy_decoder, struct vp8_segment_hdr *seg);

void decode_mvs(struct vp8_decoder_ctx       *ctx,
                struct mb_info               *current,
                const struct mb_info         *left,
                const struct mb_info         *above,
                const struct mv_clamp_rect   *bounds,
                struct bool_decoder          *entropy_decoder);

void decode_intra_mb_mode(struct mb_info         *current,
                          struct vp8_entropy_hdr *hdr,
                          struct bool_decoder    *entropy_decoder);

#endif

#ifdef __cplusplus
}
#endif
