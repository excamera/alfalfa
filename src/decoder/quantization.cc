/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include "quantization.hh"
#include "frame_header.hh"
#include "block.hh"
#include "macroblock.hh"
#include "safe_array.hh"
#include "decoder.hh"

using namespace std;

static constexpr SafeArray< uint16_t, 128 > dc_qlookup =
  {{ 4,   5,   6,   7,   8,   9,  10,  10,   11,  12,  13,  14,  15,
     16,  17,  17,  18,  19,  20,  20,  21,   21,  22,  22,  23,  23,
     24,  25,  25,  26,  27,  28,  29,  30,   31,  32,  33,  34,  35,
     36,  37,  37,  38,  39,  40,  41,  42,   43,  44,  45,  46,  46,
     47,  48,  49,  50,  51,  52,  53,  54,   55,  56,  57,  58,  59,
     60,  61,  62,  63,  64,  65,  66,  67,   68,  69,  70,  71,  72,
     73,  74,  75,  76,  76,  77,  78,  79,   80,  81,  82,  83,  84,
     85,  86,  87,  88,  89,  91,  93,  95,   96,  98, 100, 101, 102,
     104, 106, 108, 110, 112, 114, 116, 118, 122, 124, 126, 128, 130,
     132, 134, 136, 138, 140, 143, 145, 148, 151, 154, 157 }};

static constexpr SafeArray< uint16_t, 128 > ac_qlookup =
  {{ 4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,
     17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
     30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,
     43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,
     56,  57,  58,  60,  62,  64,  66,  68,  70,  72,  74,  76,  78,
     80,  82,  84,  86,  88,  90,  92,  94,  96,  98, 100, 102, 104,
     106, 108, 110, 112, 114, 116, 119, 122, 125, 128, 131, 134, 137,
     140, 143, 146, 149, 152, 155, 158, 161, 164, 167, 170, 173, 177,
     181, 185, 189, 193, 197, 201, 205, 209, 213, 217, 221, 225, 229,
     234, 239, 245, 249, 254, 259, 264, 269, 274, 279, 284 }};

static inline int16_t clamp_q( const int16_t q )
{
  if ( q < 0 ) return 0;
  if ( q > 127 ) return 127;

  return q;
}

Quantizer::Quantizer()
  : y_ac(),
    y_dc(),
    y2_ac(),
    y2_dc(),
    uv_ac(),
    uv_dc()
{}

Quantizer::Quantizer( const QuantIndices & quant_indices )
  : y_ac(  ac_qlookup.at( clamp_q( quant_indices.y_ac_qi ) ) ),
    y_dc(  dc_qlookup.at( clamp_q( quant_indices.y_ac_qi + quant_indices.y_dc.get_or( 0 ) ) ) ),
    y2_ac( ac_qlookup.at( clamp_q( quant_indices.y_ac_qi + quant_indices.y2_ac.get_or( 0 ) ) ) * 155/100 ),
    y2_dc( dc_qlookup.at( clamp_q( quant_indices.y_ac_qi + quant_indices.y2_dc.get_or( 0 ) ) ) * 2 ),
    uv_ac( ac_qlookup.at( clamp_q( quant_indices.y_ac_qi + quant_indices.uv_ac.get_or( 0 ) ) ) ),
    uv_dc( dc_qlookup.at( clamp_q( quant_indices.y_ac_qi + quant_indices.uv_dc.get_or( 0 ) ) ) )
{
  if ( y2_ac < 8 ) y2_ac = 8;
  if ( uv_dc > 132 ) uv_dc = 132;
}

DCTCoefficients DCTCoefficients::dequantize( const pair<uint16_t, uint16_t> & factors ) const
{
  DCTCoefficients new_coefficients;
  new_coefficients.at( 0 ) = coefficients_.at( 0 ) * factors.first;
  for ( uint8_t i = 1; i < 16; i++ ) {
    new_coefficients.at( i ) = coefficients_.at( i ) * factors.second;
  }

  return new_coefficients;
}

template <>
DCTCoefficients Y2Block::dequantize( const Quantizer & quantizer ) const
{
  assert( coded_ );

  return coefficients_.dequantize( quantizer.y2() );
}

template <>
DCTCoefficients YBlock::dequantize( const Quantizer & quantizer ) const
{
  return coefficients_.dequantize( quantizer.y() );
}

template <>
DCTCoefficients UVBlock::dequantize( const Quantizer & quantizer ) const
{
  return coefficients_.dequantize( quantizer.uv() );
}

DCTCoefficients DCTCoefficients::quantize( const pair<uint16_t, uint16_t> & factors ) const
{
  DCTCoefficients new_coefficients;
  new_coefficients.at( 0 ) = coefficients_.at( 0 ) / factors.first;
  for ( uint8_t i = 1; i < 16; i++ ) {
    new_coefficients.at( i ) = coefficients_.at( i ) / factors.second;
  }

  return new_coefficients;
}

template <>
DCTCoefficients Y2Block::quantize( const Quantizer & quantizer,
                                   const DCTCoefficients & coefficients )
{
  return coefficients.quantize( quantizer.y2() );
}

template <>
DCTCoefficients YBlock::quantize( const Quantizer & quantizer,
                                  const DCTCoefficients & coefficients )
{
  return coefficients.quantize( quantizer.y() );
}

template <>
DCTCoefficients UVBlock::quantize( const Quantizer & quantizer,
                                   const DCTCoefficients & coefficients)
{
  return coefficients.quantize( quantizer.uv() );
}
