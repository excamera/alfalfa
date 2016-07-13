/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

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
