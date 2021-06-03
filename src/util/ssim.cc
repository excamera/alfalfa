/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Based on (and linked with) libx264 (GPL-2+) */

/* Copyright (C) 2015-2018 the Alfalfa authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA. */

#include <cstdint>
#include <vector>
#include <x264.h>

#include "ssim.hh"

struct x264_pixel_function_t
{
  // 158 is the number of pointers in this internal x264 struct
  // this is obviously a bit fragile since x264 could add more
  void * ptrs[ 158 ];
};

// These are internal libx264 instructions so they aren't included in x264.h
extern "C"
{
  float x264_8_pixel_ssim_wxh( const x264_pixel_function_t * func, const uint8_t *pix1,
                             uintptr_t stride1, const uint8_t *pix2, uintptr_t stride2,
                             int width, int height, void *buf, int *cnt );

  void x264_8_pixel_init( int cpu, x264_pixel_function_t *pixf );

  uint32_t x264_cpu_detect( void );
}

x264_pixel_function_t init_pixel_function( void )
{
  x264_pixel_function_t pix_func;
  x264_8_pixel_init( x264_cpu_detect(), &pix_func );

  return pix_func;
}

x264_pixel_function_t x264_funcs = init_pixel_function();

double ssim( const TwoD<uint8_t> & image, const TwoD<uint8_t> & other_image )
{
   int count;
   std::vector<uint8_t> tmp_buffer;
   // Buffer size calculation taken from x264
   tmp_buffer.resize( 8 * ( image.width() / 4 + 3 ) * sizeof( int ) );

   // No padding so stride = width
   double ssim = x264_8_pixel_ssim_wxh( &x264_funcs, &image.at( 0, 0 ), image.width(),
                                        &other_image.at( 0, 0 ), other_image.width(),
                                        image.width(), image.height(),
                                        tmp_buffer.data(), &count );

   return ssim / count;
}
