#ifndef FRAME_HEADER_HH
#define FRAME_HEADER_HH

#include "vp8_header_structures.hh"
#include "bool_decoder.hh"

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

  ModeRefLFDeltaUpdate( BoolDecoder & data )
  : ref_update( data ),
    mode_update( data )
  {}
};

struct SegmentFeatureData
{
  Flag segment_feature_mode;
  Array< Flagged< Signed<7> >, 4 > quantizer_update;
  Array< Flagged< Signed<6> >, 4 > loop_filter_update;

  SegmentFeatureData( BoolDecoder & data )
    : segment_feature_mode( data ),
      quantizer_update( data ),
      loop_filter_update( data )
  {}
};

struct UpdateSegmentation
{
  Flag update_mb_segmentation_map;
  Flagged<SegmentFeatureData> segment_feature_data;
  Optional< Array< Flagged< Unsigned<8> >, 3 > > mb_segmentation_map;

  UpdateSegmentation( BoolDecoder & data )
    : update_mb_segmentation_map( data ),
      segment_feature_data( data ),
      mb_segmentation_map( update_mb_segmentation_map
			   ? decltype(mb_segmentation_map)( data )
			   : decltype(mb_segmentation_map)() )
  {}
};

struct KeyFrameHeader
{
  Flag color_space;
  Flag clamping_type;
  Flagged<UpdateSegmentation> update_segmentation;
  Flag filter_type;
  Unsigned<6> loop_filter_level;
  Unsigned<3> sharpness_level;
  Flagged<Flagged<ModeRefLFDeltaUpdate>> mode_lf_adjustments;
  Unsigned<2> log2_nbr_of_dct_partitions;
  QuantIndices quant_indices;
  Flag refresh_entropy_probs;
  Array< Array< Array< Array< Flagged< Unsigned<8> >, 11 >, 3 >, 8 >, 4 > token_prob_update;
  Flagged< Unsigned<8> > prob_skip_false;

  KeyFrameHeader( BoolDecoder & data )
    : color_space( data ), clamping_type( data ),
      update_segmentation( data ), filter_type( data ),
      loop_filter_level( data ), sharpness_level( data ),
      mode_lf_adjustments( data ),
      log2_nbr_of_dct_partitions( data ),
      quant_indices( data ),
      refresh_entropy_probs( data ),
      token_prob_update( data ),
      prob_skip_false( data )
  {}

  struct DerivedQuantities
  {
    typedef std::array< uint8_t, 3 > mb_segment_tree_probs_type;

    mb_segment_tree_probs_type mb_segment_tree_probs;
  };

  DerivedQuantities derived_quantities( void ) const;
};

#endif /* FRAME_HEADER_HH */
