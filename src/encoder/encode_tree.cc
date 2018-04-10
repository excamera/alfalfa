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

#include <vector>

#include "bool_encoder.hh"

using namespace std;

template <class enumeration, uint8_t alphabet_size, const TreeArray< alphabet_size > & nodes >
static void encode( BoolEncoder & encoder,
                    const Tree< enumeration, alphabet_size, nodes > & value,
                    const ProbabilityArray< alphabet_size > & probs )
{
  /* reverse the tree */
  SafeArray< uint8_t, 128 + nodes.size() > value_to_index;
  for ( uint8_t i = 0; i < nodes.size(); i++ ) {
    value_to_index.at( 128 + nodes.at( i ) - 1 ) = i;
  }

  vector< pair< bool, Probability > > bits;

  /* find the path to the node */
  uint8_t node_index = value_to_index.at( 128 - value - 1 );
  bits.emplace_back( node_index & 1, probs.at( node_index >> 1 ) );
  while ( node_index > 1 ) {
    node_index = value_to_index.at( 128 + (node_index & 0xfe) - 1 );
    bits.emplace_back( node_index & 1, probs.at( node_index >> 1 ) );
  }

  /* encode the path */
  for ( auto it = bits.rbegin(); it != bits.rend(); it++ ) {
    encoder.put( it->first, it->second );
  }
}
