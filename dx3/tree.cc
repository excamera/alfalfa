#include <cassert>
#include <iostream>

#include "bool_decoder.hh"

using namespace std;

template < uint8_t alphabet_size, class T >
T BoolDecoder::tree( const std::array< int8_t, 2 * (alphabet_size - 1) > & nodes,
		     const std::array< uint8_t, alphabet_size - 1 > & probabilities )
{
  /* verify correctness of tree -- XXX */
  for ( unsigned int i = 0; i < nodes.size(); i++ ) {
    auto value = nodes.at( i );
    if ( value > 0 ) {
      assert( value % 2 == 0 );
    }
  }

  int i = 0;

  while ( ( i = nodes.at( i + get( probabilities.at( i >> 1 ) ) ) ) > 0 ) {}

  assert( i <= 0 );
  /* XXX verify i is not bigger than cardinality of T */

  return static_cast< T >( -i );
}
