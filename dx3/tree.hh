#ifndef TREE_HH
#define TREE_HH

#include <cstdint>
#include <array>

#include "bool_decoder.hh"

template <uint8_t alphabet_size, class T>
class Tree
{
private:
  typedef std::array< int8_t, 2 * (alphabet_size - 1) > tree_type;
  typedef std::array< uint8_t, alphabet_size - 1 > probabilities_type;

  tree_type tree_;
  probabilities_type probabilities_;

public:
  Tree( const tree_type & s_tree,
	const probabilities_type & s_probabilities );

  T get( BoolDecoder & data ) const;
};

#endif
