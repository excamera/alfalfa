#ifndef BOOL_DECODER_HH
#define BOOL_DECODER_HH

#include "chunk.hh"
#include "safe_array.hh"

typedef int8_t TreeNode;

template < std::size_t alphabet_size >
using TreeArray = SafeArray< TreeNode, 2 * (alphabet_size - 1) >;

typedef uint8_t Probability;

template < std::size_t alphabet_size >
using ProbabilityArray = SafeArray< Probability, alphabet_size - 1 >;

class BoolDecoder
{
private:
  Chunk chunk_;
  
  uint32_t range_, value_;
  char bit_count_;

  void load_octet( void )
  {
    if ( chunk_.size() ) {
      value_ |= chunk_.octet();
      chunk_ = chunk_( 1 );
    }
  }

public:
  BoolDecoder( const Chunk & s_chunk )
    : chunk_( s_chunk ),
      range_( 255 ),
      value_( 0 ),
      bit_count_( 0 )
  {
    load_octet();
    value_ <<= 8;
    load_octet();
  }

  /* based on dixie bool_decoder.h */
  bool get( const Probability & probability )
  {
    const uint32_t split = 1 + (((range_ - 1) * probability) >> 8);
    const uint32_t SPLIT = split << 8;
    bool ret;

    if ( value_ >= SPLIT ) { /* encoded a one */
      ret = 1;
      range_ -= split;
      value_ -= SPLIT;
    } else { /* encoded a zero */
      ret = 0;
      range_ = split;
    }

    while ( range_ < 128 ) {
      value_ <<= 1;
      range_ <<= 1;
      if ( ++bit_count_ == 8 ) {
	bit_count_ = 0;
	load_octet();
      }
    }

    return ret;
  }

  bool bit( void ) { return get( 128 ); }

  uint32_t uint( const unsigned int num_bits )
  {
    uint32_t ret = 0;

    assert( num_bits < 32 );

    for ( int bit = num_bits - 1; bit >= 0; bit-- ) {
      ret |= (BoolDecoder::bit() << bit);
    }

    return ret;
  }

  int32_t sint( const unsigned int num_bits )
  {
    uint32_t ret = uint( num_bits );
    return bit() ? -ret : ret;
  }

  template < uint8_t alphabet_size, class T >
  T tree( const TreeArray< alphabet_size > & nodes,
	  const ProbabilityArray< alphabet_size > & probabilities,
	  const uint8_t starting_node = 0 );
};

#endif /* BOOL_DECODER_HH */
