#ifndef FRAME_HEADER_HH
#define FRAME_HEADER_HH

#include "optional.hh"
#include "bool_decoder.hh"

#include <array>

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

  KeyFrameHeader( BoolDecoder & data )
    : color_space( data.bit() ),
      clamping_type( data.bit() ),
      segmentation_enabled( data.bit() ),
      update_segmentation( segmentation_enabled
			   ? UpdateSegmentation( data )
			   : Optional<UpdateSegmentation>() )
  {}
};

#endif /* FRAME_HEADER_HH */
