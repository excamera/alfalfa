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

#ifndef BLOCK_HH
#define BLOCK_HH

#include <iostream>

#include "modemv_data.hh"
#include "2d.hh"
#include "vp8_raster.hh"
#include "safe_array.hh"
#include "vp8_header_structures.hh"

struct ProbabilityTables;
struct Quantizer;
class BoolEncoder;

enum BlockType { Y_after_Y2 = 0, Y2, UV, Y_without_Y2 };

constexpr static int16_t ZERO_REF[ 16 ] = { 0 };

class DCTCoefficients
{
private:
  SafeArray< int16_t, 16 > coefficients_ {{}};
public:

  void reinitialize( void )
  {
    coefficients_ = SafeArray< int16_t, 16 > {{}};
  }

  int16_t & at( const unsigned int index )
  {
    return coefficients_.at( index );
  }

  const int16_t & at( const unsigned int index ) const
  {
    return coefficients_.at( index );
  }

  void idct_add( VP8Raster::Block4 & output ) const;
  void iwht( SafeArray<SafeArray<DCTCoefficients, 4>, 4> & output ) const;

  void subtract_dct( const VP8Raster::Block4 & block, const TwoDSubRange< uint8_t, 4, 4 > & prediction );
  void wht( SafeArray< int16_t, 16 > & input );

  void set_dc_coefficient( const int16_t & val );

  bool operator==( const DCTCoefficients & other ) const
  {
    return coefficients_ == other.coefficients_;
  }

  bool operator!=( const DCTCoefficients & other ) const
  {
    return not operator==( other );
  }

  DCTCoefficients operator-( const DCTCoefficients & other ) const
  {
    DCTCoefficients result;
    for ( size_t i = 0; i < 16; i++ ) {
      result.at( i ) = at( i ) - other.at( i );
    }
    return result;

  }

  DCTCoefficients operator+( const DCTCoefficients & other ) const
  {
    DCTCoefficients result;
    for ( size_t i = 0; i < 16; i++ ) {
      result.at( i ) = at( i ) + other.at( i );
    }
    return result;

  }

  friend std::ostream &operator<<( std::ostream & output, const DCTCoefficients & dctc )
  {
    for ( size_t i = 0; i < 16; i++ ) {
      output << dctc.at( i );
      if ( i != 15 ) {
        output << ", ";
      }
    }

    return output;
  }

  DCTCoefficients dequantize( const std::pair<uint16_t, uint16_t> & factors ) const;
  DCTCoefficients quantize( const std::pair<uint16_t, uint16_t> & factors ) const;

  void zero_out()
  {
    memset( &at( 0 ), 0, 16 * sizeof( int16_t ) );
  }
};

template <BlockType initial_block_type, class PredictionMode>
class Block
{
private:
  // Keeping coefficients at base address of block reduces pointer arithmetic in accesses
  DCTCoefficients coefficients_ {};

  typename TwoD< Block >::Context context_;

  BlockType type_ { initial_block_type };

  PredictionMode prediction_mode_ {};

  bool coded_ { true };

  bool has_nonzero_ { false };

  MotionVector motion_vector_ {};

public:
  Block( const typename TwoD< Block >::Context & context )
    : context_( context )
  {}

  const PredictionMode & prediction_mode( void ) const { return prediction_mode_; }

  void set_prediction_mode( const PredictionMode prediction_mode )
  {
    prediction_mode_ = prediction_mode;
  }

  const typename TwoD< Block >::Context & context( void ) const { return context_; }

  void set_above( const Optional< const Block * > & s_above ) { context_.above = s_above; }
  void set_left( const Optional< const Block * > & s_left ) { context_.left = s_left; }

  void set_Y_without_Y2( void )
  {
    static_assert( initial_block_type == Y_after_Y2,
                   "set_Y_without_Y2 called on non-Y coded block" );
    type_ = Y_without_Y2;
  }

  void set_Y_after_Y2( void )
  {
    static_assert( initial_block_type == Y_after_Y2,
                   "set_Y_after_Y2 called on non-Y coded block" );
    type_ = Y_after_Y2;
  }

  void set_coded ( const bool coded )
  {
    coded_ = coded;
  }

  void set_if_coded( void )
  {
    static_assert( initial_block_type == Y2,
                   "set_if_coded attempted on non-Y2 coded block" );
    if ( (prediction_mode_ == B_PRED) or (prediction_mode_ == SPLITMV) ) {
      coded_ = false;
    }
  }

  void parse_tokens( BoolDecoder & data, const ProbabilityTables & probability_tables );

  BlockType type( void ) const { return type_; }
  bool coded( void ) const { static_assert( initial_block_type == Y2,
                                            "only Y2 blocks can be omitted" ); return coded_; }
  bool has_nonzero( void ) const { return has_nonzero_; }

  void set_dc_coefficient( const int16_t & val );
  DCTCoefficients dequantize( const Quantizer & quantizer ) const;
  static DCTCoefficients quantize( const Quantizer & quantizer, const DCTCoefficients & coefficients );

  const MotionVector & motion_vector() const
  {
    static_assert( initial_block_type != Y2, "Y2 blocks do not have motion vectors" );
    return motion_vector_;
  }

  void set_motion_vector( const MotionVector & other )
  {
    static_assert( initial_block_type != Y2, "Y2 blocks do not have motion vectors" );
    motion_vector_ = other;
  }

  void read_subblock_inter_prediction( BoolDecoder & data, const MotionVector & best_mv,
                                       const SafeArray< SafeArray< Probability, MV_PROB_CNT >, 2 > & motion_vector_probs );

  void write_subblock_inter_prediction( BoolEncoder & encoder, const MotionVector & best_mv,
                                        const SafeArray< SafeArray< Probability, MV_PROB_CNT >, 2 > & motion_vector_probs ) const;

  void serialize_tokens( BoolEncoder & data,
                         const ProbabilityTables & probability_tables ) const;

  DCTCoefficients & mutable_coefficients( void ) { return coefficients_; }
  const DCTCoefficients & coefficients( void ) const { return coefficients_; }

  void add_residue( VP8Raster::Block4 & raster ) const;

  void calculate_has_nonzero()
  {
    has_nonzero_ = ( memcmp( &coefficients_.at( 0 ), ZERO_REF,
                             sizeof( ZERO_REF ) ) != 0 );
  }

  void zero_out()
  {
    has_nonzero_ = false;
    coefficients_.zero_out();
  }

  bool operator==( const Block & other ) const
  {
    return type_ == other.type_ and
           coded_ == other.coded_ and
           has_nonzero_ == other.has_nonzero_ and
           coefficients_ == other.coefficients_ and
           prediction_mode_ == other.prediction_mode_ and
           motion_vector_ == other.motion_vector_;
  }

  bool operator!=( const Block & other ) const { return not operator==( other ); }
};

using Y2Block = Block< Y2, mbmode >;
using YBlock = Block< Y_after_Y2, bmode >;
using UVBlock = Block< UV, mbmode >;

#endif /* BLOCK_HH */
