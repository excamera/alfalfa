#include <cassert>

#include "tree.hh"

template <uint8_t alphabet_size, class T>
Tree<alphabet_size, T>::Tree( const Tree::tree_type & s_tree,
			      const Tree::probabilities_type & s_probabilities )
  : tree_( s_tree ),
    probabilities_( s_probabilities )
{
  for ( unsigned int i = 0; i < tree_.size(); i++ ) {
    auto value = tree_.at( i );
    if ( value > 0 ) {
      assert( value % 2 == 0 );
    }
  }
}

template <uint8_t alphabet_size, class T>
T Tree<alphabet_size, T>::get( BoolDecoder & data ) const
{
  int i = 0;

  while (i = tree_.at( i + data.get( probabilities_.at( i >> 1 ) ) ) > 0) {}

  assert( i <= 0 );
  /* XXX verify i is not bigger than cardinality of T */

  return static_cast< T >( -i );
}
