/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef TRANSFORM_SSE_HH
#define TRANSFORM_SSE_HH

extern "C" {
  void vp8_short_idct4x4llm_mmx( const short *input, unsigned char *pred,
                                 int pitch, unsigned char *dest,int stride );
}

#endif
