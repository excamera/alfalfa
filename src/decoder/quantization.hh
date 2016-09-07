/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef QUANTIZATION_HH
#define QUANTIZATION_HH

#include <cstdint>
#include <utility>

struct QuantIndices;

struct Quantizer
{
  uint16_t y_ac, y_dc, y2_ac, y2_dc, uv_ac, uv_dc;

  Quantizer( const QuantIndices & quant_indices );

  Quantizer();

  std::pair<uint16_t, uint16_t> y2() const { return { y2_dc, y2_ac }; }
  std::pair<uint16_t, uint16_t> y() const { return { y_dc, y_ac }; }
  std::pair<uint16_t, uint16_t> uv() const { return { uv_dc, uv_ac }; }
};

#endif /* QUANTIZATION_HH */
