#ifndef FRAME_HEADER_HH
#define FRAME_HEADER_HH

#include "optional.hh"
#include "bool_decoder.hh"

class UpdateSegmentation
{
public:
  bool segmentation_enabled;
  Optional<bool> update_mb_segmentation_map;
  Optional<bool> update_segment_feature_data;

  UpdateSegmentation( BoolDecoder & data );
};

class KeyFrameHeader
{
public:
  bool color_space;
  bool clamping_type;
  UpdateSegmentation update_segmentation;
  /*
  bool filter_type;
  char loop_filter_level;
  char sharpness_level;
  MB_LF_Adjustments mb_lf_adjustments;
  char log2_nbr_of_dct_partitions;
  QuantIndices quant_indices;
  bool refresh_entropy_probs;
  TokenProbUpdate token_prob_update;
  MBSkip mb_skip;
  */

  KeyFrameHeader( BoolDecoder & data );
};

#endif /* FRAME_HEADER_HH */
