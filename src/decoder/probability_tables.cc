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

#include "decoder.hh"
#include "frame.hh"

#include <boost/functional/hash.hpp>

using namespace std;

size_t ProbabilityTables::hash( void ) const
{
  size_t hash_val = 0;

  for ( auto const & block_sub : coeff_probs ) {
    for ( auto const & bands_sub : block_sub ) {
      for ( auto const & contexts_sub : bands_sub ) {
        boost::hash_range( hash_val, contexts_sub.begin(), contexts_sub.end() );
      }
    }
  }

  boost::hash_range( hash_val, y_mode_probs.begin(), y_mode_probs.end() );

  boost::hash_range( hash_val, uv_mode_probs.begin(), uv_mode_probs.end() );

  for ( auto const & sub : motion_vector_probs ) {
    boost::hash_range( hash_val, sub.begin(), sub.end() );
  }

  return hash_val;
}

void ProbabilityTables::mv_prob_update( const Enumerate<Enumerate<MVProbUpdate, MV_PROB_CNT>, 2> & mv_prob_updates )
{
  for ( uint8_t i = 0; i < mv_prob_updates.size(); i++ ) {
    for ( uint8_t j = 0; j < mv_prob_updates.at( i ).size(); j++ ) {
      const auto & prob = mv_prob_updates.at( i ).at( j );

      if ( prob.initialized() ) {
        motion_vector_probs.at( i ).at( j ) = prob.get();
      }
    }
  }
}

template <class HeaderType>
void ProbabilityTables::coeff_prob_update( const HeaderType & header )
{
  /* token probabilities (if given in frame header) */
  for ( unsigned int i = 0; i < BLOCK_TYPES; i++ ) {
    for ( unsigned int j = 0; j < COEF_BANDS; j++ ) {
      for ( unsigned int k = 0; k < PREV_COEF_CONTEXTS; k++ ) {
        for ( unsigned int l = 0; l < ENTROPY_NODES; l++ ) {
          const auto & node = header.token_prob_update.at( i ).at( j ).at( k ).at( l ).coeff_prob;
          if ( node.initialized() ) {
            coeff_probs.at( i ).at( j ).at( k ).at( l ) = node.get();
          }
        }
      }
    }
  }
}

template
void ProbabilityTables::coeff_prob_update<InterFrameHeader>(const InterFrameHeader &header);

template <>
void ProbabilityTables::update( const InterFrameHeader & header )
{
  coeff_prob_update( header );

  /* update intra-mode probabilities in inter macroblocks */
  if ( header.intra_16x16_prob.initialized() ) {
    assign( y_mode_probs, header.intra_16x16_prob.get() );
  }

  if ( header.intra_chroma_prob.initialized() ) {
    assign( uv_mode_probs, header.intra_chroma_prob.get() );
  }

  /* update motion vector component probabilities */
  for ( uint8_t i = 0; i < header.mv_prob_update.size(); i++ ) {
    for ( uint8_t j = 0; j < header.mv_prob_update.at( i ).size(); j++ ) {
      const auto & prob = header.mv_prob_update.at( i ).at( j );
      if ( prob.initialized() ) {
        motion_vector_probs.at( i ).at( j ) = prob.get();
      }
    }
  }
}

bool ProbabilityTables::operator==( const ProbabilityTables & other ) const
{
  return coeff_probs == other.coeff_probs
    and y_mode_probs == other.y_mode_probs
    and uv_mode_probs == other.uv_mode_probs
    and motion_vector_probs == other.motion_vector_probs;
}

uint32_t ProbabilityTables::serialize(EncoderStateSerializer &odata) const {
  uint32_t len = BLOCK_TYPES * COEF_BANDS * PREV_COEF_CONTEXTS * ENTROPY_NODES +
                 y_mode_probs.size() + uv_mode_probs.size() + 2 * MV_PROB_CNT;
  odata.reserve(len + 5);
  odata.put(EncoderSerDesTag::PROB_TABLE);
  odata.put(len);

  for (unsigned i = 0; i < BLOCK_TYPES; i++) {
    for (unsigned j = 0; j < COEF_BANDS; j++) {
      for (unsigned k = 0; k < PREV_COEF_CONTEXTS; k++) {
        for (unsigned l = 0; l < ENTROPY_NODES; l++) {
          odata.put(coeff_probs.at(i).at(j).at(k).at(l));
        }
      }
    }
  }

  for (unsigned i = 0; i < y_mode_probs.size(); i++) {
    odata.put(y_mode_probs.at(i));
  }

  for (unsigned i = 0; i < uv_mode_probs.size(); i++) {
    odata.put(uv_mode_probs.at(i));
  }

  for (unsigned i = 0; i < 2; i++) {
    for (unsigned j = 0; j < MV_PROB_CNT; j++) {
      odata.put(motion_vector_probs.at(i).at(j));
    }
  }

  return len + 5;
}

ProbabilityTables::ProbabilityTables(EncoderStateDeserializer &idata) {
  EncoderSerDesTag data_type = idata.get_tag();
  assert(data_type == EncoderSerDesTag::PROB_TABLE);
  (void) data_type;   // unused except in assert

  uint32_t expect_len = BLOCK_TYPES * COEF_BANDS * PREV_COEF_CONTEXTS * ENTROPY_NODES +
                        y_mode_probs.size() + uv_mode_probs.size() + 2 * MV_PROB_CNT;
  uint32_t get_len = idata.get<uint32_t>();
  assert(expect_len == get_len);
  (void) expect_len;  // uunusd except in assert
  (void) get_len;     // "

  for (unsigned i = 0; i < BLOCK_TYPES; i++) {
    for (unsigned j = 0; j < COEF_BANDS; j++) {
      for (unsigned k = 0; k < PREV_COEF_CONTEXTS; k++) {
        for (unsigned l = 0; l < ENTROPY_NODES; l++) {
          coeff_probs.at(i).at(j).at(k).at(l) = idata.get<Probability>();
        }
      }
    }
  }

  for (unsigned i = 0; i < y_mode_probs.size(); i++) {
    y_mode_probs.at(i) = idata.get<Probability>();
  }

  for (unsigned i = 0; i < uv_mode_probs.size(); i++) {
    uv_mode_probs.at(i) = idata.get<Probability>();
  }

  for (unsigned i = 0; i < 2; i++) {
    for (unsigned j = 0; j < MV_PROB_CNT; j++) {
      motion_vector_probs.at(i).at(j) = idata.get<Probability>();
    }
  }
}

template
void ProbabilityTables::coeff_prob_update<KeyFrameHeader>( const KeyFrameHeader & );
