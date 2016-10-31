#ifndef SAD_SSE_HH
#define SAD_SSE_HH

extern "C" {
  unsigned int vpx_sad16x16_sse2( const uint8_t *src, int src_stride,
                                  const uint8_t *ref, int ref_stride );
}

#endif /* SAD_SSE_HH */
