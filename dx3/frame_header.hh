#ifndef FRAME_HEADER_HH
#define FRAME_HEADER_HH

#include "optional.hh"
#include "bool_decoder.hh"

#include <array>

struct RefUpdate
{
  bool ref_frame_delta_update_flag;
  Optional<uint8_t> delta_magnitude;
  Optional<bool> delta_sign;

  RefUpdate( BoolDecoder & data )
    : ref_frame_delta_update_flag( data.bit() ),
      delta_magnitude( ref_frame_delta_update_flag
		       ? data.uint( 6 )
		       : Optional<uint8_t>() ),
      delta_sign( ref_frame_delta_update_flag
		  ? data.bit()
		  : Optional<bool>() )
  {}
};

struct ModeUpdate
{
  bool mb_mode_delta_update_flag;
  Optional<uint8_t> delta_magnitude;
  Optional<bool> delta_sign;

  ModeUpdate( BoolDecoder & data )
    : mb_mode_delta_update_flag( data.bit() ),
      delta_magnitude( mb_mode_delta_update_flag
		       ? data.uint( 6 )
		       : Optional<uint8_t>() ),
      delta_sign( mb_mode_delta_update_flag
		  ? data.bit()
		  : Optional<bool>() )
  {}
};

struct ModeRefLFDeltaUpdate
{
  std::array< RefUpdate, 4 > ref_update;
  std::array< ModeUpdate, 4 > mode_update;

  ModeRefLFDeltaUpdate( BoolDecoder & data )
    : ref_update{ { data, data, data, data } },
      mode_update{ { data, data, data, data } }
  {}
};

struct ModeLFAdjustments
{
  bool mode_ref_lf_delta_update_flag;
  Optional<ModeRefLFDeltaUpdate> mode_ref_lf_delta_update;

  ModeLFAdjustments( BoolDecoder & data )
    : mode_ref_lf_delta_update_flag( data.bit() ),
      mode_ref_lf_delta_update( mode_ref_lf_delta_update_flag
				? ModeRefLFDeltaUpdate( data )
				: Optional<ModeRefLFDeltaUpdate>() )
  {}
};

struct SegmentProbUpdate
{
  bool segment_prob_update;
  Optional<uint8_t> segment_prob;

  SegmentProbUpdate( BoolDecoder & data )
    : segment_prob_update( data.bit() ),
      segment_prob( segment_prob_update ? data.uint( 8 ) : Optional<uint8_t>() )
  {}
};

struct MBSegmentationMap
{
  std::array< SegmentProbUpdate, 3 > segment_prob_update;

  MBSegmentationMap( BoolDecoder & data )
    : segment_prob_update{ { data, data, data } }
  {}
};

struct QuantizerUpdate
{
  bool quantizer_update;
  Optional<uint8_t> quantizer_update_value;
  Optional<bool> quantizer_update_sign;

  QuantizerUpdate( BoolDecoder & data )
    : quantizer_update( data.bit() ),
      quantizer_update_value( quantizer_update ? data.uint( 7 ) : Optional<uint8_t>() ),
      quantizer_update_sign( quantizer_update ? data.bit() : Optional<bool>() )
  {}
};

struct LoopFilterUpdate
{
  bool loop_filter_update;
  Optional<uint8_t> lf_update_value;
  Optional<bool> lf_update_sign;

  LoopFilterUpdate( BoolDecoder & data )
    : loop_filter_update( data.bit() ),
      lf_update_value( loop_filter_update ? data.uint( 6 ) : Optional<uint8_t>() ),
      lf_update_sign( loop_filter_update ? data.bit() : Optional<bool>() )
  {}
};

struct SegmentFeatureData
{
  bool segment_feature_mode;
  std::array<QuantizerUpdate, 4> quantizer_update;
  std::array<LoopFilterUpdate, 4> loop_filter_update;

  SegmentFeatureData( BoolDecoder & data )
    : segment_feature_mode( data.bit() ),
      quantizer_update{ { data, data, data, data } },
      loop_filter_update{ { data, data, data, data } }
  {}
};

struct UpdateSegmentation
{
  bool update_mb_segmentation_map;
  bool update_segment_feature_data;
  Optional<SegmentFeatureData> segment_feature_data;
  Optional<MBSegmentationMap> mb_segmentation_map;

  UpdateSegmentation( BoolDecoder & data )
    : update_mb_segmentation_map( data.bit() ),
      update_segment_feature_data( data.bit() ),
      segment_feature_data( update_segment_feature_data
			    ? SegmentFeatureData( data ) :
			    Optional<SegmentFeatureData>() ),
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
  bool loop_filter_adj_enable;
  Optional<ModeLFAdjustments> mode_lf_adjustments;

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
      loop_filter_adj_enable( data.bit() ),
      mode_lf_adjustments( loop_filter_adj_enable
			   ? ModeLFAdjustments( data )
			   : Optional<ModeLFAdjustments>() )
  {}
};

#endif /* FRAME_HEADER_HH */
