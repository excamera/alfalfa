/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef PREDICTOR_SSE_HH
#define PREDICTOR_SSE_HH

extern "C" {
  typedef void predict_block_function
  (
    const uint8_t        *src_ptr,
    const unsigned int   src_pixels_per_line,
    const uint8_t        *output_ptr,
    const unsigned int   output_pitch,
    const unsigned int   output_height,
    const unsigned int   vp8_filter_index
  );

  predict_block_function vp8_filter_block1d4_h6_ssse3;
  predict_block_function vp8_filter_block1d4_v6_ssse3;
  predict_block_function vp8_filter_block1d8_h6_ssse3;
  predict_block_function vp8_filter_block1d8_v6_ssse3;
  predict_block_function vp8_filter_block1d16_h6_ssse3;
  predict_block_function vp8_filter_block1d16_v6_ssse3;

}

#endif
