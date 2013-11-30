#ifndef BOOL_DECODER_HH
#define BOOL_DECODER_HH

#include <array>

#include "chunk.hh"

typedef int8_t TreeNode;

template < std::size_t alphabet_size >
using TreeArray = std::array< TreeNode, 2 * (alphabet_size - 1) >;

typedef uint8_t Probability;

template < std::size_t alphabet_size >
using ProbabilityArray = std::array< Probability, alphabet_size - 1 >;

class BoolDecoder
{
private:
  Chunk chunk_;
  
  uint32_t range_, value_;
  char bit_count_;

  void load_octet( void );

public:
  BoolDecoder( const Chunk & s_chunk );

  bool get( const Probability & probability );
  bool bit( void ) { return get( 128 ); }
  uint32_t uint( const unsigned int num_bits );
  int32_t sint( const unsigned int num_bits );

  template < uint8_t alphabet_size, class T >
  T tree( const TreeArray< alphabet_size > & nodes,
	  const ProbabilityArray< alphabet_size > & probabilities,
	  const uint8_t starting_node = 0 );
};

#endif /* BOOL_DECODER_HH */
