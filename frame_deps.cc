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

#include "./vpx_decoder.h"
#include "./test_vector_reader.hh"

/* Reference to dixie algorithm interface in vp8_dixie_iface.c */
extern vpx_codec_iface_t vpx_codec_vp8_dixie_algo;

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
  vpx_codec_dec_cfg_t     cfg;
  if (vpx_codec_dec_init(&decoder, &vpx_codec_vp8_dixie_algo, &cfg, 0)) {
    fprintf(stderr, "Failed to initialize decoder:\n");
    return EXIT_FAILURE;
  }

  /* Read frame by frame */
  while (!test_vector_reader.read_frame(&buf, &buf_sz, &buf_alloc_sz)) {
    printf("END OF ONE FRAME \n");
  }
}
