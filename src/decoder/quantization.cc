/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Copyright 2013-2018 the Alfalfa authors
                       and the Massachusetts Institute of Technology

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

      1. Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "quantization.hh"
#include "frame_header.hh"
#include "block.hh"
#include "macroblock.hh"
#include "safe_array.hh"
#include "decoder.hh"

#ifdef HAVE_SSE2
#include <emmintrin.h>
#endif

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
  alignas( 16 ) DCTCoefficients new_coefficients;

#ifdef HAVE_SSE2

  int16_t q0 = static_cast<int16_t>( factors.first );
  int16_t q1 = static_cast<int16_t>( factors.second );

  __m128i coeffs_0 = _mm_loadu_si128( reinterpret_cast<const __m128i *>( &coefficients_.at( 0 ) ) );
  __m128i coeffs_1 = _mm_loadu_si128( reinterpret_cast<const __m128i *>( &coefficients_.at( 8 ) ) );

   __m128i factors_0 = _mm_set_epi16( q1, q1, q1, q1, q1, q1, q1, q0 );
  __m128i factors_1 = _mm_set1_epi16( q1 );

  coeffs_0 = _mm_mullo_epi16( coeffs_0, factors_0 );
  coeffs_1 = _mm_mullo_epi16( coeffs_1, factors_1 );

  _mm_store_si128( reinterpret_cast<__m128i *>( &new_coefficients.at( 0 ) ), coeffs_0 );
  _mm_store_si128( reinterpret_cast<__m128i *>( &new_coefficients.at( 8 ) ), coeffs_1 );

#else

  new_coefficients.at( 0 ) = coefficients_.at( 0 ) * factors.first;
  for ( uint8_t i = 1; i < 16; i++ ) {
    new_coefficients.at( i ) = coefficients_.at( i ) * factors.second;
  }

#endif

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
