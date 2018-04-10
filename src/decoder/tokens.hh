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

#ifndef TOKENS_HH
#define TOKENS_HH

#include "bool_decoder.hh"

class BoolEncoder;

enum token {
  ZERO_TOKEN,
  ONE_TOKEN,
  TWO_TOKEN,
  THREE_TOKEN,
  FOUR_TOKEN,
  DCT_VAL_CATEGORY1,
  DCT_VAL_CATEGORY2,
  DCT_VAL_CATEGORY3,
  DCT_VAL_CATEGORY4,
  DCT_VAL_CATEGORY5,
  DCT_VAL_CATEGORY6,
  DCT_EOB_TOKEN
};

constexpr unsigned int ENTROPY_NODES = DCT_VAL_CATEGORY6 + 1;
constexpr unsigned int MAX_ENTROPY_TOKENS = DCT_EOB_TOKEN + 1;

static constexpr SafeArray< uint8_t, 16 > coefficient_to_band {{ 0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7 }};
static constexpr SafeArray< uint8_t, 16 > zigzag = {{ 0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15 }};

template < unsigned int length >
class TokenDecoder
{
private:
  const uint16_t base_value_;
  const SafeArray< Probability, length > bit_probabilities_;

public:
  TokenDecoder( const uint16_t base_value, const SafeArray< Probability, length > & probs )
    : base_value_( base_value ), bit_probabilities_( probs ) {}

  uint16_t decode( BoolDecoder & data ) const;
  void encode( BoolEncoder & encoder, const uint16_t value ) const;
  uint16_t base_value( void ) const { return base_value_; }
  uint16_t upper_limit( void ) const { return base_value_ + (1 << length); }
};

static const TokenDecoder< 2 > token_decoder_1( 7, { { 165, 145 } } );
static const TokenDecoder< 3 > token_decoder_2( 11, { { 173, 148, 140 } } );
static const TokenDecoder< 4 > token_decoder_3( 19, { { 176, 155, 140, 135 } } );
static const TokenDecoder< 5 > token_decoder_4( 35, { { 180, 157, 141, 134, 130 } } );
static const TokenDecoder< 11 > token_decoder_5( 67, { { 254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129 } } );

#endif /* TOKENS_HH */
