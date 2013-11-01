#include <cassert>

#include "bool_decoder.hh"

template < uint8_t alphabet_size, class T >
T BoolDecoder::tree( const std::array< int8_t, 2 * (alphabet_size - 1) > & nodes,
		     const std::array< uint8_t, alphabet_size - 1 > & probabilities )
{
  int i = 0;

  while ( (i = nodes.at( i + get( probabilities.at( i >> 1 ) ) ) > 0) ) {}

  assert( i <= 0 );
  /* XXX verify i is not bigger than cardinality of T */

  return static_cast< T >( -i );
}
