#ifndef BOOL_DECODER_HH
#define BOOL_DECODER_HH

#include <array>

#include "block.hh"

class BoolDecoder
{
private:
  Block block_;
  
  uint32_t range_, value_;
  char bit_count_;

  void load_octet( void );

public:
  BoolDecoder( const Block & s_block );

  bool get( const uint8_t & probability );
  bool bit( void ) { return get( 128 ); }
  uint32_t uint( const unsigned int num_bits );
  int32_t sint( const unsigned int num_bits );

  template < uint8_t alphabet_size, class T >
  T tree( const std::array< int8_t, 2 * (alphabet_size - 1) > & nodes,
	  const std::array< uint8_t, alphabet_size - 1 > & probabilities );
};

#endif /* BOOL_DECODER_HH */
