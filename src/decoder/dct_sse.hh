#ifndef DCT_SSE_HH
#define DCT_SSE_HH

#include <cstddef>

extern "C" {
  void vp8_short_fdct4x4_sse2( short *input, short *output, int pitch );
  void vp8_short_walsh4x4_sse2( short *input, short *output, int pitch );
}

#endif /* DCT_SSE_HH */
