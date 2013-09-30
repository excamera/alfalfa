#ifndef FRAME_HEADER_HH
#define FRAME_HEADER_HH

#include "vp8_header_structures.hh"
#include "bool_decoder.hh"

#include <array>

struct QuantIndices
{
  uint8_t y_ac_qi;
  FlagMagSign<4> y_dc;
  FlagMagSign<4> y2_dc;
  FlagMagSign<4> y2_ac;
  FlagMagSign<4> uv_dc;
  FlagMagSign<4> uv_ac;

  QuantIndices( BoolDecoder & data )
    : y_ac_qi( data.uint( 7 ) ),
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
  std::array< FlaggedType<UnsignedInteger<uint8_t, 8> >, 3 > segment_prob_update;

  MBSegmentationMap( BoolDecoder & data )
    : segment_prob_update{ { data, data, data } }
  {}
};

struct SegmentFeatureData
{
  bool segment_feature_mode;
  std::array<FlagMagSign<7>, 4> quantizer_update;
  std::array<FlagMagSign<6>, 4> loop_filter_update;

  SegmentFeatureData( BoolDecoder & data )
    : segment_feature_mode( data.bit() ),
      quantizer_update{ { data, data, data, data } },
      loop_filter_update{ { data, data, data, data } }
  {}
};

struct UpdateSegmentation
{
  bool update_mb_segmentation_map;
  FlaggedType<SegmentFeatureData> segment_feature_data;
  Optional<MBSegmentationMap> mb_segmentation_map;

  UpdateSegmentation( BoolDecoder & data )
    : update_mb_segmentation_map( data.bit() ),
      segment_feature_data( data ),
      mb_segmentation_map( update_mb_segmentation_map
			   ? MBSegmentationMap( data )
			   : Optional<MBSegmentationMap>() )
  {}
};

struct KeyFrameHeader
{
  bool color_space;
  bool clamping_type;
  bool segmentation_enabled;
  Optional<UpdateSegmentation> update_segmentation;
  bool filter_type;
  uint8_t loop_filter_level;
  uint8_t sharpness_level;
  FlaggedType<FlaggedType<ModeRefLFDeltaUpdate>> mode_lf_adjustments;
  uint8_t log2_nbr_of_dct_partitions;
  QuantIndices quant_indices;

  KeyFrameHeader( BoolDecoder & data )
    : color_space( data.bit() ),
      clamping_type( data.bit() ),
      segmentation_enabled( data.bit() ),
      update_segmentation( segmentation_enabled
			   ? UpdateSegmentation( data )
			   : Optional<UpdateSegmentation>() ),
      filter_type( data.bit() ),
      loop_filter_level( data.uint( 6 ) ),
      sharpness_level( data.uint( 3 ) ),
      mode_lf_adjustments( data ),
      log2_nbr_of_dct_partitions( data.uint( 2 ) ),
      quant_indices( data )
  {}
};

#endif /* FRAME_HEADER_HH */
