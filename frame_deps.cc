/*
 *  Copyright (c) 2013 Anirudh Sivaraman
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/* This is a simple program that reads ivf files (the VP8 test vector format)
 * and prints out frame dependencies
 */

#include <assert.h>
#include <vector>

#include "./vpx_decoder.hh"
#include "./test_vector_reader.hh"
#include "./vp8_dixie_iface.hh"
#include "./dixie.hh"
#include "./frame_state.hh"
#include "./operator_parser.hh"

int main(int argc, const char** argv) {
  /* Read IVF file name from command line */
  assert(argc == 2);
  std::string file_name(argv[1]);
  TestVectorReader test_vector_reader(file_name);

  /* Set up buffers for operators */
  uint8_t  *operator_buffer = NULL;
  uint32_t op_buf_sz = 0, op_buf_alloc_sz = 0;

  /* Intialize decoder */
  vpx_codec_ctx_t         decoder = {nullptr, nullptr, VPX_CODEC_OK, nullptr, 0, nullptr, nullptr};
  if (vp8_init(&decoder)) {
    fprintf(stderr, "Failed to initialize decoder:\n");
    return EXIT_FAILURE;
  }

  /* Create vector of frame states */
  std::vector<FrameState> frame_states;
  struct vp8_decoder_ctx* dixie_ctx = &(decoder.priv->alg_priv->decoder_ctx);

  /* Set up current raster reference ids */
  struct vp8_raster_ref_ids current_raster_ref_ids = {0,0,0};

  /* Read frame by frame */
  while (!test_vector_reader.read_frame(&operator_buffer, &op_buf_sz, &op_buf_alloc_sz)) {
    /* Create op_parser with current raster references */
    OperatorParser op_parser(dixie_ctx,
                             static_cast<unsigned char*>(operator_buffer),
                             op_buf_sz,
                             current_raster_ref_ids);

    /* Decode operator headers alone */
    op_parser.decode_operator_headers();

    /* Decode macroblock prediction records */
    op_parser.decode_macroblock_data();

    /* Pretty print */
    frame_states.push_back(op_parser.get_frame_state());
    frame_states.back().pretty_print_compact();

    /* Update ids of different raster buffers for the next frame */
    current_raster_ref_ids = op_parser.update_ref_rasters();

  }
}
