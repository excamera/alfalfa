#include <vector>

#include "bool_encoder.hh"
#include "vp8_header_structures.hh"

template <class enumeration, uint8_t alphabet_size, const TreeArray< alphabet_size > & nodes >
static void encode( BoolEncoder & encoder,
		    const Tree< enumeration, alphabet_size, nodes > & value,
		    const ProbabilityArray< alphabet_size > & probs )
{
  /* reverse the tree */
  SafeArray< uint8_t, alphabet_size + nodes.size() > value_to_index;
  for ( uint8_t i = 0; i < nodes.size(); i++ ) {
    value_to_index.at( alphabet_size + nodes.at( i ) - 1 ) = i;
  }

  std::vector< std::pair< bool, TreeNode > > bits;

  /* find the path to the node */
  uint8_t node_index = value_to_index.at( alphabet_size - value - 1 );
  while ( node_index ) {
    const bool bit = node_index & 1;
    node_index = value_to_index.at( alphabet_size + (node_index & 0xfe) - 1 );
    bits.emplace_back( bit, node_index >> 1 );
  }

  /* encode the path */
  for ( auto it = bits.rbegin(); it != bits.rend(); it++ ) {
    encoder.put( it->first, probs.at( it->second ) );
  }
}
