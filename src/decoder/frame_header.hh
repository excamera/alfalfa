#ifndef FRAME_HEADER_HH
#define FRAME_HEADER_HH

#include "vp8_header_structures.hh"
#include "bool_decoder.hh"
#include "vp8_prob_data.hh"
#include "modemv_data.hh"
#include "exception.hh"
#include "quantization.hh"
#include "loopfilter.hh"

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
};

struct ModeRefLFDeltaUpdate
{
  Array< Flagged< Signed<6> >, 4 > ref_update;
  Array< Flagged< Signed<6> >, 4 > mode_update;

  ModeRefLFDeltaUpdate( BoolDecoder & data ) : ref_update( data ), mode_update( data ) {}
};

struct SegmentFeatureData
{
  Flag segment_feature_mode;
  Array< Flagged< Signed<7> >, 4 > quantizer_update;
  Array< Flagged< Signed<6> >, 4 > loop_filter_update;

  SegmentFeatureData( BoolDecoder & data )
    : segment_feature_mode( data ), quantizer_update( data ), loop_filter_update( data ) {}
};

struct UpdateSegmentation
{
  Flag update_mb_segmentation_map;
  Flagged<SegmentFeatureData> segment_feature_data;
  Optional< Array< Flagged< Unsigned<8> >, 3 > > mb_segmentation_map;

  UpdateSegmentation( BoolDecoder & data )
    : update_mb_segmentation_map( data ), segment_feature_data( data ),
      mb_segmentation_map( update_mb_segmentation_map, data ) {}
};

struct TokenProbUpdate
{
  Flagged< Unsigned<8> > coeff_prob;

  TokenProbUpdate( BoolDecoder & data,
		   const unsigned int l, const unsigned int k,
		   const unsigned int j, const unsigned int i )
    : coeff_prob( data, k_coeff_entropy_update_probs.at( i ).at( j ).at( k ).at( l ) ) {}

  TokenProbUpdate( const bool initialized, const uint8_t x ) : coeff_prob( initialized, x ) {}
};

struct MVProbUpdate
{
  Flagged< Unsigned<7> > mv_prob;

  bool initialized( void ) const { return mv_prob.initialized(); }
  uint8_t get( void ) const { return mv_prob.get() << 1; }

  MVProbUpdate( BoolDecoder & data,
		const unsigned int j, const unsigned int i )
    : mv_prob( data, k_mv_entropy_update_probs.at( i ).at( j ) )
  {}
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
  Flagged< Array< Unsigned<8>, 4 > > intra_16x16_prob;
  Flagged< Array< Unsigned<8>, 3 > > intra_chroma_prob;
  Enumerate< Enumerate< MVProbUpdate, MV_PROB_CNT >, 2 > mv_prob_update;

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
};

#endif /* FRAME_HEADER_HH */
