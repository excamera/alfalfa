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

#ifndef FRAME_HH
#define FRAME_HH

#include "2d.hh"
#include "block.hh"
#include "macroblock.hh"

struct References;
struct Segmentation;
struct FilterAdjustments;

struct Quantizers
{
  Quantizer quantizer;
  SafeArray< Quantizer, num_segments > segment_quantizers;
};

template <class FrameHeaderType, class MacroblockType>
class Frame
{
 private:
  bool show_;

  unsigned int display_width_, display_height_;
  unsigned int macroblock_width_ { VP8Raster::macroblock_dimension( display_width_ ) },
    macroblock_height_ { VP8Raster::macroblock_dimension( display_height_ ) };

  TwoD<Y2Block> Y2_ { macroblock_width_, macroblock_height_ };
  TwoD<YBlock> Y_ { 4 * macroblock_width_, 4 * macroblock_height_ };
  TwoD<UVBlock> U_ { 2 * macroblock_width_, 2 * macroblock_height_ };
  TwoD<UVBlock> V_ { 2 * macroblock_width_, 2 * macroblock_height_ };

  FrameHeaderType header_;

  Optional<TwoD<MacroblockType>> macroblock_headers_ {};

  ProbabilityArray< num_segments > calculate_mb_segment_tree_probs( void ) const;
  SafeArray< Quantizer, num_segments > calculate_segment_quantizers( const Optional< Segmentation > & segmentation ) const;

  std::vector< uint8_t > serialize_first_partition( const ProbabilityTables & probability_tables ) const;
  std::vector< std::vector< uint8_t > > serialize_tokens( const ProbabilityTables & probability_tables ) const;

 public:
  void relink_y2_blocks( void );
  void loopfilter( const Optional< Segmentation > & segmentation,
                   const Optional< FilterAdjustments > & quantizer_filter_adjustments,
                   VP8Raster & target ) const;

  Frame( const bool show,
         const unsigned int width,
         const unsigned int height,
         BoolDecoder & first_partition );

  const FrameHeaderType & header() const { return header_; }
  FrameHeaderType & mutable_header() { return header_; }

  const TwoD<MacroblockType> & macroblocks() const { return macroblock_headers_.get(); }
  TwoD<MacroblockType> & mutable_macroblocks() { return macroblock_headers_.get(); }

  void parse_macroblock_headers( BoolDecoder & rest_of_first_partition,
                                 const ProbabilityTables & probability_tables,
                                 const bool error_concealment );

  void update_segmentation( SegmentationMap & mutable_segmentation_map );

  void parse_tokens( std::vector< Chunk > dct_partitions, const ProbabilityTables & probability_tables );

  void decode( const Optional< Segmentation > & segmentation, const References & references,
               VP8Raster & raster ) const;

  void copy_to( const RasterHandle & raster, References & references ) const;

  std::string reference_update_stats( void ) const;

  std::string stats( void ) const;

  std::vector< uint8_t > serialize( const ProbabilityTables & probability_tables ) const;

  uint8_t dct_partition_count( void ) const { return 1 << header_.log2_number_of_dct_partitions; }

  bool show_frame( void ) const { return show_; }

  void set_show( bool show_frame ) { show_ = show_frame; }

  bool operator==( const Frame & other ) const;

  unsigned int display_width() const { return display_width_; }
  unsigned int display_height() const { return display_height_; }

  /* allow moving */
  Frame( Frame && other ) noexcept;
  Frame & operator=( Frame && other );

  /* empty frame */
  Frame( const uint16_t width, const uint16_t height );
};

using KeyFrame = Frame<KeyFrameHeader, KeyFrameMacroblock>;
using InterFrame = Frame<InterFrameHeader, InterFrameMacroblock>;

#endif /* FRAME_HH */
