/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

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

  bool valid_;
  bool complete_chunk_;

  void load_octet( void )
  {
    if ( chunk_.size() ) {
      value_ |= chunk_.octet();
      chunk_ = chunk_( 1 );
    }
    else if ( not complete_chunk_ ) {
      valid_ = false;
    }
  }

public:
  BoolDecoder( const Chunk & s_chunk, const bool complete_chunk = true )
    : chunk_( s_chunk ),
      range_( 255 ),
      value_( 0 ),
      bit_count_( 0 ),
      valid_( true ),
      complete_chunk_( complete_chunk )
  {
    load_octet();
    value_ <<= 8;
    load_octet();
  }

  /* based on dixie bool_decoder.h */
  bool get( const Probability probability = 128 )
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

  bool valid() const { return valid_; }

  static BoolDecoder & zero_decoder()
  {
    static BoolDecoder zd { { nullptr, 0 } };
    return zd;
  }

  template < uint8_t alphabet_size, class T >
  T tree( const TreeArray< alphabet_size > & nodes,
          const ProbabilityArray< alphabet_size > & probabilities );
};

template <class enumeration, uint8_t alphabet_size, const TreeArray< alphabet_size > & nodes>
class Tree
{
private:
  enumeration value_;

public:
  typedef enumeration type;

  Tree( BoolDecoder & data, const ProbabilityArray< alphabet_size > & probabilities )
    : value_( data.tree< alphabet_size, enumeration >( nodes, probabilities ) )
  {}

  Tree( const enumeration & x ) : value_( x ) {}

  operator const enumeration & () const { return value_; }
};

#endif /* BOOL_DECODER_HH */
