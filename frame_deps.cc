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

#include "./vpx_decoder.h"
#include "./test_vector_reader.hh"
#include "./vp8_dixie_iface.h"
#include "./dixie.h"
#include "./frame_state.hh"

int main(int argc, const char** argv) {
  /* Read IVF file name from command line */
  assert(argc == 2);
  std::string file_name(argv[1]);
  TestVectorReader test_vector_reader(file_name);

  /* Set up buffers */
  uint8_t  *buf = NULL;
  uint32_t buf_sz = 0, buf_alloc_sz = 0;

  /* Intialize decoder */
  vpx_codec_ctx_t         decoder;
  if (vp8_init(&decoder)) {
    fprintf(stderr, "Failed to initialize decoder:\n");
    return EXIT_FAILURE;
  }

  /* Read frame by frame */
  std::vector<FrameState> frame_states;
  int frame_in=0;
  struct vp8_decoder_ctx* dixie_ctx = &(decoder.priv->alg_priv->decoder_ctx);
  while (!test_vector_reader.read_frame(&buf, &buf_sz, &buf_alloc_sz)) {
    ++frame_in;
    decode_frame(dixie_ctx, static_cast<unsigned char*>(buf), buf_sz);
    printf("Decoded frame %d.\n", frame_in);
    printf("Pretty prining state: \n");
    frame_states.push_back(FrameState(dixie_ctx->frame_hdr,
                                      dixie_ctx->segment_hdr,
                                      dixie_ctx->loopfilter_hdr,
                                      dixie_ctx->token_hdr,
                                      dixie_ctx->quant_hdr,
                                      dixie_ctx->reference_hdr,
                                      dixie_ctx->entropy_hdr));
    frame_states.back().pretty_print_frame_hdr();
    frame_states.back().pretty_print_segment_hdr();
    frame_states.back().pretty_print_loopfilter_hdr();
    frame_states.back().pretty_print_token_hdr();
    frame_states.back().pretty_print_quant_hdr();
    frame_states.back().pretty_print_reference_hdr();
    frame_states.back().pretty_print_frame_deps();
    frame_states.back().pretty_print_entropy_hdr();    
    printf("\n END OF ONE FRAME \n\n\n\n");
  }
}
