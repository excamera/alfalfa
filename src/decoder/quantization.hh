#ifndef QUANTIZATION_HH
#define QUANTIZATION_HH

#include <cstdint>

#include "optional.hh"

struct QuantIndices;
struct UpdateSegmentation;

struct QuantizerAdjustment
{
  bool absolute { false };
  int8_t value { 0 };

  QuantizerAdjustment() {}

  QuantizerAdjustment( const uint8_t segment_id,
		       const Optional< UpdateSegmentation > & update_segmentation );

  void update( const uint8_t segment_id,
	       const Optional< UpdateSegmentation > & update_segmentation );
};

struct Quantizer
{
  uint8_t y_ac, y_dc, y2_ac, y2_dc, uv_ac, uv_dc;

  Quantizer( const QuantIndices & quant_indices,
	     const QuantizerAdjustment & adjustment = QuantizerAdjustment() );
};

#endif /* QUANTIZATION_HH */
