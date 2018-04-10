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

#include "loopfilter.hh"
#include "frame_header.hh"
#include "macroblock.hh"
#include "vp8_raster.hh"
#include "loopfilter_filters.hh"
#include "decoder.hh"

static inline uint8_t clamp63( const int input )
{
  if ( input > 63 ) return 63;
  if ( input < 0 ) return 0;
  return input;
}

FilterParameters::FilterParameters( const bool use_simple_filter,
                                    const uint8_t s_filter_level,
                                    const uint8_t s_sharpness_level )
  : type( use_simple_filter ? LoopFilterType::Simple : LoopFilterType::Normal ),
    filter_level( s_filter_level ),
    sharpness_level( s_sharpness_level )
{}

FilterParameters::FilterParameters()
  : type(),
    filter_level(),
    sharpness_level()
{}

static int8_t mode_adjustment( const SafeArray< int8_t, 4 > & mode_adjustments,
                               const reference_frame macroblock_reference_frame,
                               const mbmode macroblock_y_mode )
{
  if ( macroblock_reference_frame == CURRENT_FRAME ) {
    return ( macroblock_y_mode == B_PRED ) ? mode_adjustments.at( 0 ) : 0;
  } else if ( macroblock_y_mode == ZEROMV ) {
    return mode_adjustments.at( 1 );
  } else if ( macroblock_y_mode == SPLITMV ) {
    return mode_adjustments.at( 3 );
  } else {
    return mode_adjustments.at( 2 );
  }
}

void FilterParameters::adjust( const SafeArray< int8_t, num_reference_frames > & ref_adjustments,
                               const SafeArray< int8_t, 4 > & mode_adjustments,
                               const reference_frame macroblock_reference_frame,
                               const mbmode macroblock_y_mode )
{
  filter_level += ref_adjustments.at( macroblock_reference_frame )
    + mode_adjustment( mode_adjustments, macroblock_reference_frame, macroblock_y_mode );
}

SimpleLoopFilter::SimpleLoopFilter( const FilterParameters & params )
  : interior_limit_vector_(),
    macroblock_limit_vector_(),
    subblock_limit_vector_(),
    filter_level_( clamp63( params.filter_level ) )
{
  uint8_t interior_limit = filter_level_;
  if ( params.sharpness_level ) {
    interior_limit >>= params.sharpness_level > 4 ? 2 : 1;

    if ( interior_limit > 9 - params.sharpness_level ) {
      interior_limit = 9 - params.sharpness_level;
    }
  }

  if ( interior_limit < 1 ) {
    interior_limit = 1;
  }

  interior_limit_vector_.fill(interior_limit);

  macroblock_limit_vector_.fill(( ( filter_level_ + 2 ) * 2 ) + interior_limit);

  subblock_limit_vector_.fill(( filter_level_ * 2 ) + interior_limit);
}

NormalLoopFilter::NormalLoopFilter( const bool key_frame,
                                    const FilterParameters & params )
  : simple_( params ),
    hev_threshold_vector_()
{
  assert( params.type == LoopFilterType::Normal );

  uint8_t high_edge_variance_threshold(simple_.filter_level() >= 15);

  if ( simple_.filter_level() >= 40 ) {
    high_edge_variance_threshold++;
  }

  if ( simple_.filter_level() >= 20 and (not key_frame) ) {
    high_edge_variance_threshold++;
  }
  hev_threshold_vector_.fill(high_edge_variance_threshold);

}

void SimpleLoopFilter::filter( VP8Raster::Macroblock & , const bool )
{
  throw Unsupported( "VP8 'simple' in-loop deblocking filter" );
}

// Corresponds roughly to vp8_loop_filter_mbh_c combined with vp8_loop_filter_row_normal
void NormalLoopFilter::filter( VP8Raster::Macroblock & raster, const bool skip_subblock_edges )
{
  /* 1: filter the left inter-macroblock edge */
  if ( raster.Y.column() > 0 ) {
    filter_mb_vertical( raster );
  }

  /* 2: filter the vertical subblock edges */
  if ( not skip_subblock_edges ) {
    filter_sb_vertical( raster );
  }

  /* 3: filter the top inter-macroblock edge */
  if ( raster.Y.row() > 0 ) {
    filter_mb_horizontal( raster );
  }

  /* 4: filter the horizontal subblock edges */
  if ( not skip_subblock_edges ) {
    filter_sb_horizontal( raster );
  }
}

// Roughly the same as filter_mbloop_filter_horizontal_edge_c
template <class BlockType>
void NormalLoopFilter::filter_mb_vertical_c( BlockType & block )
{
  const uint8_t size = BlockType::dimension;

  for ( unsigned int row = 0; row < size; row++ ) {
    uint8_t *central = &block.at( 0, row );

    const int8_t mask = vp8_filter_mask( simple_.interior_limit(),
                                         simple_.macroblock_edge_limit(),
                                         *(central - 4),
                                         *(central - 3),
                                         *(central - 2),
                                         *(central - 1),
                                         *(central),
                                         *(central + 1),
                                         *(central + 2),
                                         *(central + 3) );

    const int8_t hev = vp8_hevmask( hev_threshold_vector_[0],
                                    *(central - 2),
                                    *(central - 1),
                                    *(central),
                                    *(central + 1) );

    vp8_mbfilter( mask, hev,
                  *(central - 3), *(central - 2), *(central - 1),
                  *(central), *(central + 1), *(central + 2) );
  }
}

void NormalLoopFilter::filter_mb_vertical( VP8Raster::Macroblock & raster )
{
#ifdef HAVE_SSE2
  uint8_t *y_ptr = &raster.Y.at(0, 0);
  uint8_t *u_ptr = &raster.U.at(0, 0);
  uint8_t *v_ptr = &raster.V.at(0, 0);

  const int y_stride = raster.Y.stride();
  const int uv_stride = raster.U.stride(); // Is v stride always u stride?

  auto blimit_vec = simple_.macroblock_limit_vector().data();
  auto limit_vec = simple_.interior_limit_vector().data();

  vp8_mbloop_filter_vertical_edge_sse2(y_ptr, y_stride, blimit_vec, limit_vec, hev_threshold_vector_.data());
  vp8_mbloop_filter_vertical_edge_uv_sse2(u_ptr, uv_stride, blimit_vec, limit_vec, hev_threshold_vector_.data(),
                                          v_ptr);
#else
  filter_mb_vertical_c( raster.Y );
  filter_mb_vertical_c( raster.U );
  filter_mb_vertical_c( raster.V );
#endif
}

template <class BlockType>
void NormalLoopFilter::filter_mb_horizontal_c( BlockType & block )
{
  const uint8_t size = BlockType::dimension;

  const unsigned int stride = block.stride();

  for ( unsigned int column = 0; column < size; column++ ) {
    uint8_t *central = &block.at( column, 0 );

    const int8_t mask = vp8_filter_mask( simple_.interior_limit(),
                                         simple_.macroblock_edge_limit(),
                                         *(central - 4 * stride ),
                                         *(central - 3 * stride ),
                                         *(central - 2 * stride ),
                                         *(central - stride ),
                                         *(central),
                                         *(central + stride ),
                                         *(central + 2 * stride ),
                                         *(central + 3 * stride ) );

    const int8_t hev = vp8_hevmask( hev_threshold_vector_[0],
                                    *(central - 2 * stride ),
                                    *(central - stride ),
                                    *(central),
                                    *(central + stride ) );

    vp8_mbfilter( mask, hev,
                  *(central - 3 * stride ),
                  *(central - 2 * stride ),
                  *(central - stride ),
                  *(central),
                  *(central + stride ),
                  *(central + 2 * stride ) );
  }
}

void NormalLoopFilter::filter_mb_horizontal( VP8Raster::Macroblock & raster )
{
#ifdef HAVE_SSE2
  uint8_t *y_ptr = &raster.Y.at(0, 0);
  uint8_t *u_ptr = &raster.U.at(0, 0);
  uint8_t *v_ptr = &raster.V.at(0, 0);

  const int y_stride = raster.Y.stride();
  const int uv_stride = raster.U.stride(); // Is v stride always u stride?

  auto blimit_vec = simple_.macroblock_limit_vector().data();
  auto limit_vec = simple_.interior_limit_vector().data();

  vp8_mbloop_filter_horizontal_edge_sse2(y_ptr, y_stride, blimit_vec, limit_vec, hev_threshold_vector_.data());
  vp8_mbloop_filter_horizontal_edge_uv_sse2(u_ptr, uv_stride, blimit_vec, limit_vec, hev_threshold_vector_.data(),
                                          v_ptr);
#else
  filter_mb_horizontal_c( raster.Y );
  filter_mb_horizontal_c( raster.U );
  filter_mb_horizontal_c( raster.V );
#endif
}

template <class BlockType>
void NormalLoopFilter::filter_sb_vertical_c( BlockType & block )
{
  const uint8_t size = BlockType::dimension;

  for ( unsigned int center_column = 4; center_column < size; center_column += 4 ) {
    for ( unsigned int row = 0; row < size; row++ ) {
      uint8_t *central = &block.at( center_column, row );

      const int8_t mask = vp8_filter_mask( simple_.interior_limit(),
                                           simple_.subblock_edge_limit(),
                                           *(central - 4),
                                           *(central - 3),
                                           *(central - 2),
                                           *(central - 1),
                                           *(central),
                                           *(central + 1),
                                           *(central + 2),
                                           *(central + 3) );

      const int8_t hev = vp8_hevmask( hev_threshold_vector_[0],
                                      *(central - 2),
                                      *(central - 1),
                                      *(central),
                                      *(central + 1) );

      vp8_filter( mask, hev,
                  *(central - 2), *(central - 1),
                  *(central), *(central + 1) );
    }
  }
}

void NormalLoopFilter::filter_sb_vertical( VP8Raster::Macroblock & raster )
{
#ifdef HAVE_SSE2
  uint8_t *y_ptr = &raster.Y.at(0, 0);
  uint8_t *u_ptr = &raster.U.at(0, 0);
  uint8_t *v_ptr = &raster.V.at(0, 0);

  const int y_stride = raster.Y.stride();
  const int uv_stride = raster.U.stride(); // Is v stride always u stride?

  auto blimit_vec = simple_.subblock_limit_vector().data();
  auto limit_vec = simple_.interior_limit_vector().data();

#ifdef ARCH_X86_64
    vp8_loop_filter_bv_y_sse2(y_ptr, y_stride, blimit_vec, limit_vec, hev_threshold_vector_.data(), 2);
#else
    vp8_loop_filter_vertical_edge_sse2(y_ptr + 4, y_stride, blimit_vec, limit_vec, hev_threshold_vector_.data());
    vp8_loop_filter_vertical_edge_sse2(y_ptr + 8, y_stride, blimit_vec, limit_vec, hev_threshold_vector_.data());
    vp8_loop_filter_vertical_edge_sse2(y_ptr + 12, y_stride, blimit_vec, limit_vec, hev_threshold_vector_.data());
#endif

  vp8_loop_filter_vertical_edge_uv_sse2(u_ptr + 4, uv_stride, blimit_vec, limit_vec, hev_threshold_vector_.data(), v_ptr + 4);
#else
  filter_sb_vertical_c( raster.Y );
  filter_sb_vertical_c( raster.U );
  filter_sb_vertical_c( raster.V );
#endif
}

template <class BlockType>
void NormalLoopFilter::filter_sb_horizontal_c( BlockType & block )
{
  const uint8_t size = BlockType::dimension;

  const unsigned int stride = block.stride();

  for ( unsigned int center_row = 4; center_row < size; center_row += 4 ) {
    for ( unsigned int column = 0; column < size; column++ ) {
      uint8_t *central = &block.at( column, center_row );

      const int8_t mask = vp8_filter_mask( simple_.interior_limit(),
                                           simple_.subblock_edge_limit(),
                                           *(central - 4 * stride ),
                                           *(central - 3 * stride ),
                                           *(central - 2 * stride ),
                                           *(central - stride ),
                                           *(central),
                                           *(central + stride ),
                                           *(central + 2 * stride ),
                                           *(central + 3 * stride ) );

      const int8_t hev = vp8_hevmask( hev_threshold_vector_[0],
                                      *(central - 2 * stride ),
                                      *(central - stride ),
                                      *(central),
                                      *(central + stride ) );

      vp8_filter( mask, hev,
                  *(central - 2 * stride ),
                  *(central - stride ),
                  *(central),
                  *(central + stride ) );
    }
  }
}

void NormalLoopFilter::filter_sb_horizontal( VP8Raster::Macroblock & raster )
{
#ifdef HAVE_SSE2
  uint8_t *y_ptr = &raster.Y.at(0, 0);
  uint8_t *u_ptr = &raster.U.at(0, 0);
  uint8_t *v_ptr = &raster.V.at(0, 0);

  const int y_stride = raster.Y.stride();
  const int uv_stride = raster.U.stride(); // Is v stride always u stride?

  auto blimit_vec = simple_.subblock_limit_vector().data();
  auto limit_vec = simple_.interior_limit_vector().data();

#ifdef ARCH_X86_64
    vp8_loop_filter_bh_y_sse2(y_ptr, y_stride, blimit_vec, limit_vec, hev_threshold_vector_.data(), 2);
#else
    vp8_loop_filter_horizontal_edge_sse2(y_ptr + 4 * ystride, y_stride, blimit_vec, limit_vec, hev_threshold_vector_.data());
    vp8_loop_filter_horizontal_edge_sse2(y_ptr + 8 * ystride, y_stride, blimit_vec, limit_vec, hev_threshold_vector_.data());
    vp8_loop_filter_horizontal_edge_sse2(y_ptr + 12 * ystride, y_stride, blimit_vec, limit_vec, hev_threshold_vector_.data());
#endif

  vp8_loop_filter_horizontal_edge_uv_sse2(u_ptr + 4 * uv_stride, uv_stride, blimit_vec, limit_vec, hev_threshold_vector_.data(), v_ptr + 4 * uv_stride);
#else
  filter_sb_horizontal_c( raster.Y );
  filter_sb_horizontal_c( raster.U );
  filter_sb_horizontal_c( raster.V );
#endif
}
