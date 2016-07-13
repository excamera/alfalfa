/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

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
