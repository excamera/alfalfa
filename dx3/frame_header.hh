#ifndef FRAME_HEADER_HH
#define FRAME_HEADER_HH

#include "vp8_header_structures.hh"
#include "bool_decoder.hh"

#include <array>

struct QuantIndices
{
  Unsigned<7> y_ac_qi;
  FlagMagSign<4> y_dc;
  FlagMagSign<4> y2_dc;
  FlagMagSign<4> y2_ac;
  FlagMagSign<4> uv_dc;
  FlagMagSign<4> uv_ac;

  QuantIndices( BoolDecoder & data )
    : y_ac_qi( data ),
      y_dc( data ),
      y2_dc( data ),
      y2_ac( data ),
      uv_dc( data ),
      uv_ac( data )
  {}
};

struct ModeRefLFDeltaUpdate
{
  std::array< FlagMagSign<6>, 4 > ref_update;
  std::array< FlagMagSign<6>, 4 > mode_update;

  ModeRefLFDeltaUpdate( BoolDecoder & data )
    : ref_update{ { data, data, data, data } },
      mode_update{ { data, data, data, data } }
  {}
};

struct MBSegmentationMap
{
  std::array< FlaggedType<Unsigned<8>>, 3 > segment_prob_update;

  MBSegmentationMap( BoolDecoder & data )
    : segment_prob_update{ { data, data, data } }
  {}
};

struct SegmentFeatureData
{
  Bool segment_feature_mode;
  std::array<FlagMagSign<7>, 4> quantizer_update;
  std::array<FlagMagSign<6>, 4> loop_filter_update;

  SegmentFeatureData( BoolDecoder & data )
    : segment_feature_mode( data ),
      quantizer_update{ { data, data, data, data } },
      loop_filter_update{ { data, data, data, data } }
  {}
};

struct UpdateSegmentation
{
  Bool update_mb_segmentation_map;
  FlaggedType<SegmentFeatureData> segment_feature_data;
  Optional<MBSegmentationMap> mb_segmentation_map;

  UpdateSegmentation( BoolDecoder & data )
    : update_mb_segmentation_map( data ),
      segment_feature_data( data ),
      mb_segmentation_map( update_mb_segmentation_map
			   ? MBSegmentationMap( data )
			   : Optional<MBSegmentationMap>() )
  {}
};

struct KeyFrameHeader
{
  Bool color_space;
  Bool clamping_type;
  FlaggedType<UpdateSegmentation> update_segmentation;
  Bool filter_type;
  Unsigned<6> loop_filter_level;
  Unsigned<3> sharpness_level;
  FlaggedType<FlaggedType<ModeRefLFDeltaUpdate>> mode_lf_adjustments;
  Unsigned<2> log2_nbr_of_dct_partitions;
  QuantIndices quant_indices;

  KeyFrameHeader( BoolDecoder & data )
    : color_space( data ),
      clamping_type( data ),
      update_segmentation( data ),
      filter_type( data ),
      loop_filter_level( data ),
      sharpness_level( data ),
      mode_lf_adjustments( data ),
      log2_nbr_of_dct_partitions( data ),
      quant_indices( data )
  {}
};

#endif /* FRAME_HEADER_HH */
