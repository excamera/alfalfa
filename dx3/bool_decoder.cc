#include <cassert>

#include "bool_decoder.hh"
#include "exception.hh"

BoolDecoder::BoolDecoder( const Block & s_block )
  : block_( s_block ),
    range_( 255 ),
    value_( 0 ),
    bit_count_( 0 )
{
  load_octet();
  value_ <<= 8;
  load_octet();
}

void BoolDecoder::load_octet( void )
{
  if ( block_.size() ) {
    value_ |= block_.octet();
    block_ = block_( 1 );
  }
}

/* based on dixie bool_decoder.h */
bool BoolDecoder::get( const Probability & probability )
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

uint32_t BoolDecoder::uint( const unsigned int num_bits )
{
  uint32_t ret = 0;

  assert( num_bits < 32 );

  for ( int bit = num_bits - 1; bit >= 0; bit-- ) {
    ret |= (BoolDecoder::bit() << bit);
  }

  return ret;
}

int32_t BoolDecoder::sint( const unsigned int num_bits )
{
  uint32_t ret = uint( num_bits );
  return bit() ? -ret : ret;
}
