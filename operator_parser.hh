/*
 *  Copyright (c) 2013 Anirudh Sivaraman
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/* Parse header of each Operator to update FrameState
 *
 */


#ifndef OPERATOR_PARSER_HH_
#define OPERATOR_PARSER_HH_

#include "./frame_state.hh"
#include "./dixie.h"

class OperatorParser {
 public:
  /* Constructor */
  OperatorParser(struct vp8_decoder_ctx*   t_ctx,
                 const unsigned char*      t_data,
                 unsigned int              t_size,
                 struct vp8_raster_ref_ids t_raster_ref_ids);

  /* Decode Operator headers alone */
  void decode_operator_headers(void);

  /* Decode Operator's frame itself */
  void decode_frame(void);

  /* Update reference raster ids */
  struct vp8_raster_ref_ids update_ref_rasters(void);

  /* Create FrameState (full) out of ctx object
     after decoding operator header */
  FrameState get_frame_state(void);

 private:
  /* decoder_ctx object */
  struct vp8_decoder_ctx* ctx;

  /* frame_buffer ids of golden, altref, and last rasters */
  struct vp8_raster_ref_ids raster_ref_ids_;

  /* Current raster number */
  unsigned int raster_num;

  /* Pointer to data in current coded frame */
  const unsigned char* data;

  /* Current location in operator */
  unsigned int sz;
};

#endif  // OPERATOR_PARSER_HH_
