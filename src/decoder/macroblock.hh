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

#ifndef MB_RECORDS_HH
#define MB_RECORDS_HH

#include "vp8_header_structures.hh"
#include "frame_header.hh"
#include "2d.hh"
#include "block.hh"
#include "vp8_raster.hh"
#include "decoder.hh"

struct ProbabilityTables;
struct References;
class BoolEncoder;
class ReferenceUpdater;
class Scorer;

typedef SafeArray<SafeArray<SafeArray<SafeArray<std::pair<uint32_t, uint32_t>,
                                                ENTROPY_NODES >,
                                      PREV_COEF_CONTEXTS >,
                            COEF_BANDS >,
                  BLOCK_TYPES > TokenBranchCounts;

template <class FrameHeaderType, class MacroblockHeaderType>
class Macroblock
{
private:
  typename TwoD< Macroblock >::Context context_;

  Optional< Tree< uint8_t, num_segments, segment_id_tree > > segment_id_update_;
  uint8_t segment_id_;

  Optional< Boolean > mb_skip_coeff_;

  MacroblockHeaderType header_;

  Y2Block & Y2_;
  TwoDSubRange< YBlock, 4, 4 > Y_;
  TwoDSubRange< UVBlock, 2, 2 > U_, V_;

  bool has_nonzero_ { false };

  void decode_prediction_modes( BoolDecoder & data,
                                const ProbabilityTables & probability_tables,
                                const bool error_concealment );

  void encode_prediction_modes( BoolEncoder & encoder,
                                const ProbabilityTables & probability_tables ) const;

  void apply_walsh( const Quantizer & quantizer, VP8Raster::Macroblock & raster ) const;

public:
  Macroblock( const typename TwoD< Macroblock >::Context & c,
              BoolDecoder & data,
              const FrameHeaderType & key_frame_header,
              const ProbabilityArray< num_segments > & mb_segment_tree_probs,
              const ProbabilityTables & probability_tables,
              TwoD< Y2Block > & frame_Y2,
              TwoD< YBlock > & frame_Y,
              TwoD< UVBlock > & frame_U,
              TwoD< UVBlock > & frame_V,
              const bool error_concealment );

  void update_segmentation( SegmentationMap & mutable_segmentation_map );

  void parse_tokens( BoolDecoder & data,
                     const ProbabilityTables & probability_tables );

  void reconstruct_intra( const Quantizer & quantizer, VP8Raster::Macroblock & raster ) const;
  void reconstruct_inter( const Quantizer & quantizer,
                          const References & references,
                          VP8Raster::Macroblock & raster ) const;

  void loopfilter( const Optional< FilterAdjustments > & filter_adjustments,
                   const FilterParameters & loopfilter,
                   VP8Raster::Macroblock & raster ) const;

  const MacroblockHeaderType & header( void ) const { return header_; }
        MacroblockHeaderType & mutable_header( void ) { return header_; }

  const typename TwoD< Macroblock >::Context & context() const { return context_; }

  const MotionVector & base_motion_vector( void ) const;
  void set_base_motion_vector( const MotionVector & mv );

  const mbmode & uv_prediction_mode( void ) const { return U_.at( 0, 0 ).prediction_mode(); }
  const mbmode & y_prediction_mode( void ) const { return Y2_.prediction_mode(); }

  bool inter_coded() const;

  Optional< Tree< uint8_t, num_segments, segment_id_tree > > & mutable_segment_id_update( void ) { return segment_id_update_; }
  uint8_t segment_id() const { return segment_id_; }

  void serialize( BoolEncoder & encoder,
                  const FrameHeaderType & frame_header,
                  const ProbabilityArray< num_segments > & mb_segment_tree_probs,
                  const ProbabilityTables & probability_tables ) const;

  void serialize_tokens( BoolEncoder & encoder,
                         const ProbabilityTables & probability_tables ) const;

  void accumulate_token_branches( TokenBranchCounts & counts ) const;

  Scorer motion_vector_census();

  bool operator==( const Macroblock & other ) const
  {
    return has_nonzero_ == other.has_nonzero_ and
           mb_skip_coeff_ == other.mb_skip_coeff_ and
           segment_id_ == other.segment_id_;
  }

  bool operator!=( const Macroblock & other ) const { return not operator==( other ); }

  static bmode implied_subblock_mode( const mbmode y_mode )
  {
    switch ( y_mode ) {
    case DC_PRED: return B_DC_PRED;
    case V_PRED:  return B_VE_PRED;
    case H_PRED:  return B_HE_PRED;
    case TM_PRED: return B_TM_PRED;
    default: throw LogicError();
    }
  }

  /* for encoder use */
  Y2Block & Y2()                      { return Y2_; }
  TwoDSubRange< YBlock, 4, 4 >  & Y() { return Y_; }
  TwoDSubRange< UVBlock, 2, 2 > & U() { return U_; }
  TwoDSubRange< UVBlock, 2, 2 > & V() { return V_; }

  const Y2Block & Y2()                      const { return Y2_; }
  const TwoDSubRange< YBlock, 4, 4 >  & Y() const { return Y_; }
  const TwoDSubRange< UVBlock, 2, 2 > & U() const { return U_; }
  const TwoDSubRange< UVBlock, 2, 2 > & V() const { return V_; }

  bool has_nonzero() const { return has_nonzero_; }
  void calculate_has_nonzero();

  void zero_out();
};

struct KeyFrameMacroblockHeader
{
  /* no fields beyond what's in every macroblock */

  KeyFrameMacroblockHeader( BoolDecoder &, const KeyFrameHeader & ) {}
  KeyFrameMacroblockHeader() {}

  reference_frame reference( void ) const { return CURRENT_FRAME; }
};

struct InterFrameMacroblockHeader
{
  Boolean is_inter_mb;
  Optional< Boolean > mb_ref_frame_sel1;
  Optional< Boolean > mb_ref_frame_sel2;

  bool motion_vectors_flipped_; /* derived quantity */

  Optional< Tree< uint8_t, 4, split_mv_tree > > partition_id {};

  InterFrameMacroblockHeader( BoolDecoder & data, const InterFrameHeader & frame_header );
  InterFrameMacroblockHeader();

  reference_frame reference( void ) const;
  void set_reference( const reference_frame ref );
};

using KeyFrameMacroblock = Macroblock<KeyFrameHeader, KeyFrameMacroblockHeader>;
using InterFrameMacroblock = Macroblock<InterFrameHeader, InterFrameMacroblockHeader>;

#endif /* MB_RECORDS_HH */
