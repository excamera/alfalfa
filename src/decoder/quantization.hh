#ifndef QUANTIZATION_HH
#define QUANTIZATION_HH

#include <cstdint>

struct QuantIndices;

struct Quantizer
{
  uint16_t y_ac, y_dc, y2_ac, y2_dc, uv_ac, uv_dc;

  Quantizer( const QuantIndices & quant_indices );

  Quantizer();
};

#endif /* QUANTIZATION_HH */
