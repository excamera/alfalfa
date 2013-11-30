#include <cassert>
#include <iostream>

#include "bool_decoder.hh"

using namespace std;

template < uint8_t alphabet_size, class T >
T BoolDecoder::tree( const TreeArray< alphabet_size > & nodes,
		     const ProbabilityArray< alphabet_size > & probabilities,
		     const uint8_t starting_node )
{
  /* verify correctness of tree -- XXX */
  for ( unsigned int i = 0; i < nodes.size(); i++ ) {
    auto value = nodes.at( i );
    if ( value > 0 ) {
      assert( value % 2 == 0 );
    }
  }

  assert( starting_node < nodes.size() );

  int i = starting_node;

  while ( ( i = nodes.at( i + get( probabilities.at( i >> 1 ) ) ) ) > 0 ) {}

  assert( i <= 0 );
  /* XXX verify i is not bigger than cardinality of T */

  return static_cast< T >( -i );
}
