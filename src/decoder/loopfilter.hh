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

#ifndef LOOPFILTER_HH
#define LOOPFILTER_HH

#include <config.h>

#include <cstdint>

#include "optional.hh"
#include "modemv_data.hh"
#include "vp8_raster.hh"

struct KeyFrameHeader;
struct UpdateSegmentation;

enum class LoopFilterType : char { Normal, Simple, NoFilter };

struct FilterParameters
{
  LoopFilterType type;
  int filter_level; /* don't clamp until ready to make the filter */
  uint8_t sharpness_level;

  FilterParameters( const bool use_simple_filter,
                    const uint8_t s_filter_level,
                    const uint8_t s_sharpness_level );

  void adjust( const SafeArray< int8_t, num_reference_frames > & ref_adjustments,
               const SafeArray< int8_t, 4 > & mode_adjustments,
               const reference_frame macroblock_reference_frame,
               const mbmode macroblock_y_mode );

  FilterParameters();
};

class SimpleLoopFilter
{
private:
  // libvpx SSE2 routines expect preloaded vectors for arguments rather than pointers
  // to single elements
  alignas(16) std::array<uint8_t, 16> interior_limit_vector_;
  alignas(16) std::array<uint8_t, 16> macroblock_limit_vector_;
  alignas(16) std::array<uint8_t, 16> subblock_limit_vector_;
  uint8_t filter_level_;

public:
  SimpleLoopFilter( const FilterParameters & params );

  uint8_t filter_level( void ) const { return filter_level_; }
  uint8_t interior_limit( void ) const { return interior_limit_vector_[0]; }
  const std::array<uint8_t, 16>& interior_limit_vector( void ) const { return interior_limit_vector_; }
  uint8_t macroblock_edge_limit( void ) const { return macroblock_limit_vector_[0]; }
  const std::array<uint8_t, 16>& macroblock_limit_vector( void ) const { return macroblock_limit_vector_; }
  uint8_t subblock_edge_limit( void ) const { return subblock_limit_vector_[0]; }
  const std::array<uint8_t, 16>& subblock_limit_vector( void ) const { return subblock_limit_vector_; }

  void filter( VP8Raster::Macroblock & raster, const bool skip_subblock_edges );
};

class NormalLoopFilter
{
private:
  SimpleLoopFilter simple_;
  alignas(16) std::array<uint8_t, 16> hev_threshold_vector_;

  void filter_mb_vertical( VP8Raster::Macroblock & raster );

  void filter_mb_horizontal( VP8Raster::Macroblock & raster );

  void filter_sb_vertical( VP8Raster::Macroblock & raster );

  void filter_sb_horizontal( VP8Raster::Macroblock & raster );

  template <class BlockType>
  void filter_mb_vertical_c( BlockType & block );

  template <class BlockType>
  void filter_mb_horizontal_c( BlockType & block );

  template <class BlockType>
  void filter_sb_vertical_c( BlockType & block );

  template <class BlockType>
  void filter_sb_horizontal_c( BlockType & block );

public:
  NormalLoopFilter( const bool key_frame, const FilterParameters & params );

  void filter( VP8Raster::Macroblock & raster, const bool skip_subblock_edges );
};

#endif /* LOOPFILTER_HH */
