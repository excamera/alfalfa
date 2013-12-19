#ifndef QUANTIZATION_HH
#define QUANTIZATION_HH

#include <cstdint>

#include "optional.hh"

class QuantIndices;
class UpdateSegmentation;

struct Quantizer
{
  uint8_t y_ac, y_dc, y2_ac, y2_dc, uv_ac, uv_dc;

  Quantizer( const QuantIndices & quant_indices );
  Quantizer( const uint8_t segment_id,
	     const QuantIndices & quant_indices,
	     const Optional< UpdateSegmentation > & update_segmentation );
};

#endif /* QUANTIZATION_HH */
