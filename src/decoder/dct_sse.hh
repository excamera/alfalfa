#ifndef DCT_SSE_HH
#define DCT_SSE_HH

#include <cstddef>

extern "C" {
  void vp8_short_fdct4x4_sse2( short *input, short *output, int pitch );
  void vp8_short_walsh4x4_sse2( short *input, short *output, int pitch );
  void vp8_short_inv_walsh4x4_sse2( const short *input, short *output );

  void vpx_subtract_block_sse2( int rows, int cols,
                                int16_t *diff, std::ptrdiff_t diff_stride,
                                const uint8_t *src, std::ptrdiff_t src_stride,
                                const uint8_t *pred, std::ptrdiff_t pred_stride );

}

#endif /* DCT_SSE_HH */
