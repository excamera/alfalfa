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

#ifndef FRAME_HEADER_HH
#define FRAME_HEADER_HH

#include "vp8_header_structures.hh"
#include "exception.hh"

struct ProbabilityTables;

struct QuantIndices
{
  Unsigned<7> y_ac_qi;
  Flagged< Signed<4> > y_dc;
  Flagged< Signed<4> > y2_dc;
  Flagged< Signed<4> > y2_ac;
  Flagged< Signed<4> > uv_dc;
  Flagged< Signed<4> > uv_ac;

  QuantIndices( BoolDecoder & data )
    : y_ac_qi( data ), y_dc( data ), y2_dc( data ),
      y2_ac( data ), uv_dc( data ), uv_ac( data )
  {}

  QuantIndices(); /* for encoder use */

  bool operator==( const QuantIndices & other ) const
  {
    return y_ac_qi == other.y_ac_qi and
           y_dc == other.y_dc and
           y2_dc == other.y2_dc and
           y2_ac == other.y2_ac and
           uv_dc == other.uv_dc and
           uv_ac == other.uv_ac;
  }

};

struct ModeRefLFDeltaUpdate
{
  Array< Flagged< Signed<6> >, 4 > ref_update;
  Array< Flagged< Signed<6> >, 4 > mode_update;

  ModeRefLFDeltaUpdate( BoolDecoder & data ) : ref_update( data ), mode_update( data ) {}

  ModeRefLFDeltaUpdate() : ref_update(), mode_update() {}

  bool operator==( const ModeRefLFDeltaUpdate & other ) const
  {
    return ref_update == other.ref_update and
           mode_update == other.mode_update;
  }
};

struct SegmentFeatureData
{
  Flag segment_feature_mode;
  Array< Flagged< Signed<7> >, 4 > quantizer_update;
  Array< Flagged< Signed<6> >, 4 > loop_filter_update;

  SegmentFeatureData( BoolDecoder & data )
    : segment_feature_mode( data ), quantizer_update( data ), loop_filter_update( data ) {}

  SegmentFeatureData()
    : segment_feature_mode(),
      quantizer_update(),
      loop_filter_update()
  {}

  bool operator==( const SegmentFeatureData & other ) const
  {
    return segment_feature_mode == other.segment_feature_mode and
           quantizer_update == other.quantizer_update and
           loop_filter_update == other.loop_filter_update;
  }
};

struct UpdateSegmentation
{
  Flag update_mb_segmentation_map;
  Flagged<SegmentFeatureData> segment_feature_data;
  Optional< Array< Flagged< Unsigned<8> >, 3 > > mb_segmentation_map;

  UpdateSegmentation( BoolDecoder & data )
    : update_mb_segmentation_map( data ), segment_feature_data( data ),
      mb_segmentation_map( update_mb_segmentation_map, data ) {}

  UpdateSegmentation()
    : update_mb_segmentation_map(),
      segment_feature_data(),
      mb_segmentation_map()
  {}

  bool operator==( const UpdateSegmentation & other ) const
  {
    return update_mb_segmentation_map == other.update_mb_segmentation_map and
           segment_feature_data == other.segment_feature_data and
           mb_segmentation_map == other.mb_segmentation_map;
  }
};

struct TokenProbUpdate
{
  Flagged< Unsigned<8> > coeff_prob;

  TokenProbUpdate( BoolDecoder & data,
                   const unsigned int l, const unsigned int k,
                   const unsigned int j, const unsigned int i )
    : coeff_prob( data, k_coeff_entropy_update_probs.at( i ).at( j ).at( k ).at( l ) ) {}

  TokenProbUpdate( const bool initialized, const uint8_t x ) : coeff_prob( initialized, x ) {}

  TokenProbUpdate() : coeff_prob() {}

  bool operator==( const TokenProbUpdate & other ) const
  {
    return coeff_prob == other.coeff_prob;
  }
};

struct MVProbUpdate
{
  Flagged< Unsigned<7> > mv_prob;

  bool initialized( void ) const { return mv_prob.initialized(); }
  uint8_t get( void ) const { return read_half_prob( mv_prob.get() ); }

  MVProbUpdate( BoolDecoder & data,
                const unsigned int j, const unsigned int i )
    : mv_prob( data, k_mv_entropy_update_probs.at( i ).at( j ) )
  {}

  MVProbUpdate( const bool initialized, const uint8_t x )
    : mv_prob( initialized, write_prob( x ) )
  {
    if ( initialized ) {
      if ( get() != x ) {
        throw Invalid( "can't MVProbUpdate to " + std::to_string( x ) );
      }
    }
  }

  MVProbUpdate() : mv_prob() {}

  bool operator==( const MVProbUpdate & other ) const
  {
    return mv_prob == other.mv_prob;
  }

  void clear() { mv_prob.clear(); }

  static uint8_t read_half_prob( const uint8_t half_prob )
  {
    return half_prob ? ( half_prob << 1 ) : 1;
  }

  static uint8_t write_prob( const uint8_t prob )
  {
    return prob >> 1;
  }

  static bool is_settable_value( const uint8_t prob )
  {
    return ( prob == read_half_prob( write_prob( prob ) ) );
  }
};

struct KeyFrameHeader
{
  Flag color_space;
  Flag clamping_type;
  Flagged< UpdateSegmentation > update_segmentation;
  Flag filter_type;
  Unsigned<6> loop_filter_level;
  Unsigned<3> sharpness_level;
  Flagged< Flagged< ModeRefLFDeltaUpdate > > mode_lf_adjustments;
  Unsigned<2> log2_number_of_dct_partitions;
  QuantIndices quant_indices;
  Flag refresh_entropy_probs;
  Enumerate< Enumerate< Enumerate< Enumerate< TokenProbUpdate,
                                              ENTROPY_NODES >,
                                   PREV_COEF_CONTEXTS >,
                        COEF_BANDS >,
             BLOCK_TYPES > token_prob_update;
  Flagged< Unsigned<8> > prob_skip_false;

  KeyFrameHeader( BoolDecoder & data )
    : color_space( data ), clamping_type( data ),
      update_segmentation( data ), filter_type( data ),
      loop_filter_level( data ), sharpness_level( data ),
      mode_lf_adjustments( data ), log2_number_of_dct_partitions( data ),
      quant_indices( data ), refresh_entropy_probs( data ),
      token_prob_update( data ), prob_skip_false( data )
  {
    if ( color_space or clamping_type ) {
      throw Unsupported( "VP8 color_space and clamping_type bits" );
    }

    if ( filter_type ) {
      throw Unsupported( "VP8 'simple' in-loop deblocking filter" );
    }
  }

  static constexpr bool key_frame( void ) { return true; }

  bool operator==( const KeyFrameHeader & other ) const
  {
    return color_space == other.color_space and
           clamping_type == other.clamping_type and
           update_segmentation == other.update_segmentation and
           filter_type == other.filter_type and
           loop_filter_level == other.loop_filter_level and
           sharpness_level == other.sharpness_level and
           mode_lf_adjustments == other.mode_lf_adjustments and
           log2_number_of_dct_partitions == other.log2_number_of_dct_partitions and
           quant_indices == other.quant_indices and
           token_prob_update == other.token_prob_update and
           prob_skip_false == other.prob_skip_false;
  }
};

struct InterFrameHeader
{
  Flagged< UpdateSegmentation > update_segmentation;
  Flag filter_type;
  Unsigned<6> loop_filter_level;
  Unsigned<3> sharpness_level;
  Flagged< Flagged< ModeRefLFDeltaUpdate > > mode_lf_adjustments;
  Unsigned<2> log2_number_of_dct_partitions;
  QuantIndices quant_indices;
  Flag refresh_golden_frame;
  Flag refresh_alternate_frame;
  Optional< Unsigned<2> > copy_buffer_to_golden;
  Optional< Unsigned<2> > copy_buffer_to_alternate;
  Flag sign_bias_golden;
  Flag sign_bias_alternate;
  Flag refresh_entropy_probs;
  Flag refresh_last;
  Enumerate< Enumerate< Enumerate< Enumerate< TokenProbUpdate,
                                              ENTROPY_NODES >,
                                   PREV_COEF_CONTEXTS >,
                        COEF_BANDS >,
             BLOCK_TYPES > token_prob_update;
  Flagged< Unsigned<8> > prob_skip_false;
  Unsigned<8> prob_inter; /* RFC 6386 calls this prob_intra in the text, prob_inter in the code */
  Unsigned<8> prob_references_last;
  Unsigned<8> prob_references_golden;
  Flagged<Array<Unsigned<8>, 4>> intra_16x16_prob;
  Flagged<Array<Unsigned<8>, 3>> intra_chroma_prob;
  Enumerate<Enumerate<MVProbUpdate, MV_PROB_CNT>, 2> mv_prob_update;

  InterFrameHeader( BoolDecoder & data )
    : update_segmentation( data ), filter_type( data ),
      loop_filter_level( data ), sharpness_level( data ),
      mode_lf_adjustments( data ), log2_number_of_dct_partitions( data ),
      quant_indices( data ), refresh_golden_frame( data ), refresh_alternate_frame( data ),
      copy_buffer_to_golden( not refresh_golden_frame, data ),
      copy_buffer_to_alternate( not refresh_alternate_frame, data ),
      sign_bias_golden( data ), sign_bias_alternate( data ),
      refresh_entropy_probs( data ), refresh_last( data ),
    token_prob_update( data ), prob_skip_false( data ),
    prob_inter( data ), prob_references_last( data ), prob_references_golden( data ),
    intra_16x16_prob( data ), intra_chroma_prob( data ),
    mv_prob_update( data )
  {
    if ( filter_type ) {
      throw Unsupported( "VP8 'simple' in-loop deblocking filter" );
    }
  }

  static constexpr bool key_frame( void ) { return false; }

  bool operator==( const InterFrameHeader & other ) const
  {
    return update_segmentation == other.update_segmentation and
           filter_type == other.filter_type and
           loop_filter_level == other.loop_filter_level and
           sharpness_level == other.sharpness_level and
           mode_lf_adjustments == other.mode_lf_adjustments and
           log2_number_of_dct_partitions == other.log2_number_of_dct_partitions and
           quant_indices == other.quant_indices and
           refresh_golden_frame == other.refresh_golden_frame and
           refresh_alternate_frame == other.refresh_alternate_frame and
           copy_buffer_to_golden == other.copy_buffer_to_golden and
           copy_buffer_to_alternate == other.copy_buffer_to_alternate and
           sign_bias_golden == other.sign_bias_golden and
           sign_bias_alternate == other.sign_bias_alternate and
           refresh_entropy_probs == other.refresh_entropy_probs and
           refresh_last == other.refresh_last and
           token_prob_update == other.token_prob_update and
           prob_skip_false == other.prob_skip_false and
           prob_inter == other.prob_inter and
           prob_references_last == other.prob_references_last and
           prob_references_golden == other.prob_references_golden and
           intra_16x16_prob == other.intra_16x16_prob and
           intra_chroma_prob == other.intra_chroma_prob and
           mv_prob_update == other.mv_prob_update;
  }
};

#endif /* FRAME_HEADER_HH */
