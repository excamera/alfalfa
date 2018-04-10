/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Copyright 2013-2018 the Alfalfa authors
                       and the Massachusetts Institute of Technology

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

      1. Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#ifndef LOOPFILTER_FILTERS_HH
#define LOOPFILTER_FILTERS_HH

/* Excerpted from libvpx/vp8/common/loopfilter_filters.c with SSE2
 * prototypes from libvpx/vp8/common/x86/loopfilter_x86.c */

/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <config.h>
#include <cstdint>

typedef uint8_t uc;

static inline int8_t vp8_signed_char_clamp(int t)
{
    t = (t < -128 ? -128 : t);
    t = (t > 127 ? 127 : t);
    return static_cast<int8_t>( t );
}

/* should we apply any filter at all ( 11111111 yes, 00000000 no) */
signed inline char vp8_filter_mask(uc limit, uc blimit,
                                   uc p3, uc p2, uc p1, uc p0,
                                   uc q0, uc q1, uc q2, uc q3)
{
    signed char mask = 0;
    mask |= (abs(p3 - p2) > limit);
    mask |= (abs(p2 - p1) > limit);
    mask |= (abs(p1 - p0) > limit);
    mask |= (abs(q1 - q0) > limit);
    mask |= (abs(q2 - q1) > limit);
    mask |= (abs(q3 - q2) > limit);
    mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2  > blimit);
    return mask - 1;
}

/* is there high variance internal edge ( 11111111 yes, 00000000 no) */
signed inline char vp8_hevmask(uc thresh, uc p1, uc p0, uc q0, uc q1)
{
    signed char hev = 0;
    hev  |= (abs(p1 - p0) > thresh) * -1;
    hev  |= (abs(q1 - q0) > thresh) * -1;
    return hev;
}

void inline vp8_filter(signed char mask, uc hev, uc & op1,
                       uc & op0, uc & oq0, uc & oq1)

{
    signed char ps0, qs0;
    signed char ps1, qs1;
    signed char filter_value, Filter1, Filter2;
    signed char u;

    ps1 = (signed char) op1 ^ 0x80;
    ps0 = (signed char) op0 ^ 0x80;
    qs0 = (signed char) oq0 ^ 0x80;
    qs1 = (signed char) oq1 ^ 0x80;

    /* add outer taps if we have high edge variance */
    filter_value = vp8_signed_char_clamp(ps1 - qs1);
    filter_value &= hev;

    /* inner taps */
    filter_value = vp8_signed_char_clamp(filter_value + 3 * (qs0 - ps0));
    filter_value &= mask;

    /* save bottom 3 bits so that we round one side +4 and the other +3
     * if it equals 4 we'll set to adjust by -1 to account for the fact
     * we'd round 3 the other way
     */
    Filter1 = vp8_signed_char_clamp(filter_value + 4);
    Filter2 = vp8_signed_char_clamp(filter_value + 3);
    Filter1 >>= 3;
    Filter2 >>= 3;
    u = vp8_signed_char_clamp(qs0 - Filter1);
    oq0 = u ^ 0x80;
    u = vp8_signed_char_clamp(ps0 + Filter2);
    op0 = u ^ 0x80;
    filter_value = Filter1;

    /* outer tap adjustments */
    filter_value += 1;
    filter_value >>= 1;
    filter_value &= ~hev;

    u = vp8_signed_char_clamp(qs1 - filter_value);
    oq1 = u ^ 0x80;
    u = vp8_signed_char_clamp(ps1 + filter_value);
    op1 = u ^ 0x80;

}

void inline vp8_mbfilter(signed char mask, uc hev,
                         uc & op2, uc & op1, uc & op0, uc & oq0, uc & oq1, uc & oq2)
{
    signed char s, u;
    signed char filter_value, Filter1, Filter2;
    signed char ps2 = (signed char) op2 ^ 0x80;
    signed char ps1 = (signed char) op1 ^ 0x80;
    signed char ps0 = (signed char) op0 ^ 0x80;
    signed char qs0 = (signed char) oq0 ^ 0x80;
    signed char qs1 = (signed char) oq1 ^ 0x80;
    signed char qs2 = (signed char) oq2 ^ 0x80;

    /* add outer taps if we have high edge variance */
    filter_value = vp8_signed_char_clamp(ps1 - qs1);
    filter_value = vp8_signed_char_clamp(filter_value + 3 * (qs0 - ps0));
    filter_value &= mask;

    Filter2 = filter_value;
    Filter2 &= hev;

    /* save bottom 3 bits so that we round one side +4 and the other +3 */
    Filter1 = vp8_signed_char_clamp(Filter2 + 4);
    Filter2 = vp8_signed_char_clamp(Filter2 + 3);
    Filter1 >>= 3;
    Filter2 >>= 3;
    qs0 = vp8_signed_char_clamp(qs0 - Filter1);
    ps0 = vp8_signed_char_clamp(ps0 + Filter2);


    /* only apply wider filter if not high edge variance */
    filter_value &= ~hev;
    Filter2 = filter_value;

    /* roughly 3/7th difference across boundary */
    u = vp8_signed_char_clamp((63 + Filter2 * 27) >> 7);
    s = vp8_signed_char_clamp(qs0 - u);
    oq0 = s ^ 0x80;
    s = vp8_signed_char_clamp(ps0 + u);
    op0 = s ^ 0x80;

    /* roughly 2/7th difference across boundary */
    u = vp8_signed_char_clamp((63 + Filter2 * 18) >> 7);
    s = vp8_signed_char_clamp(qs1 - u);
    oq1 = s ^ 0x80;
    s = vp8_signed_char_clamp(ps1 + u);
    op1 = s ^ 0x80;

    /* roughly 1/7th difference across boundary */
    u = vp8_signed_char_clamp((63 + Filter2 * 9) >> 7);
    s = vp8_signed_char_clamp(qs2 - u);
    oq2 = s ^ 0x80;
    s = vp8_signed_char_clamp(ps2 + u);
    op2 = s ^ 0x80;
}

/* should we apply any filter at all ( 11111111 yes, 00000000 no) */
static inline signed char vp8_simple_filter_mask(uc blimit, uc p1, uc p0, uc q0, uc q1)
{
/* Why does this cause problems for win32?
 * error C2143: syntax error : missing ';' before 'type'
 *  (void) limit;
 */
    signed char mask = (abs(p0 - q0) * 2 + abs(p1 - q1) / 2  <= blimit) * -1;
    return mask;
}

static inline void vp8_simple_filter(signed char mask, uc *op1, uc *op0, uc *oq0, uc *oq1)
{
    signed char filter_value, Filter1, Filter2;
    signed char p1 = (signed char) * op1 ^ 0x80;
    signed char p0 = (signed char) * op0 ^ 0x80;
    signed char q0 = (signed char) * oq0 ^ 0x80;
    signed char q1 = (signed char) * oq1 ^ 0x80;
    signed char u;

    filter_value = vp8_signed_char_clamp(p1 - q1);
    filter_value = vp8_signed_char_clamp(filter_value + 3 * (q0 - p0));
    filter_value &= mask;

    /* save bottom 3 bits so that we round one side +4 and the other +3 */
    Filter1 = vp8_signed_char_clamp(filter_value + 4);
    Filter1 >>= 3;
    u = vp8_signed_char_clamp(q0 - Filter1);
    *oq0  = u ^ 0x80;

    Filter2 = vp8_signed_char_clamp(filter_value + 3);
    Filter2 >>= 3;
    u = vp8_signed_char_clamp(p0 + Filter2);
    *op0 = u ^ 0x80;
}

/* SSE2 prototypes */
#ifdef HAVE_SSE2

extern "C" {
  typedef void loop_filter_function
  (
      unsigned char *u,   /* source pointer */
      int p,              /* pitch */
      const uint8_t *blimit,
      const uint8_t *limit,
      const uint8_t *thresh,
      int count
  );
  
  typedef void loop_filter_ncfunction
  (
      unsigned char *u,   /* source pointer */
      int p,              /* pitch */
      const uint8_t *blimit,
      const uint8_t *limit,
      const uint8_t *thresh
  );
  
  typedef void loop_filter_uvfunction
  (
      unsigned char *u,   /* source pointer */
      int p,              /* pitch */
      const uint8_t *blimit,
      const uint8_t *limit,
      const uint8_t *thresh,
      unsigned char *v
  );
  
#ifdef ARCH_X86_64
  loop_filter_function vp8_loop_filter_bv_y_sse2;
  loop_filter_function vp8_loop_filter_bh_y_sse2;
#else
  loop_filter_ncfunction vp8_loop_filter_vertical_edge_sse2;
  loop_filter_ncfunction vp8_loop_filter_horizontal_edge_sse2;
#endif
  
  loop_filter_ncfunction vp8_mbloop_filter_vertical_edge_sse2;
  loop_filter_ncfunction vp8_mbloop_filter_horizontal_edge_sse2;
  
  loop_filter_uvfunction vp8_loop_filter_horizontal_edge_uv_sse2;
  loop_filter_uvfunction vp8_loop_filter_vertical_edge_uv_sse2;
  loop_filter_uvfunction vp8_mbloop_filter_horizontal_edge_uv_sse2;
  loop_filter_uvfunction vp8_mbloop_filter_vertical_edge_uv_sse2;
}

#endif /* HAVE_SSE2 */

#endif /* LOOPFILTER_FILTERS_HH */
