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
bool BoolDecoder::get( const uint8_t & probability )
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

uint32_t BoolDecoder::get_uint( const unsigned int num_bits )
{
  uint32_t ret = 0;

  assert( num_bits < 32 );

  for ( int bit = num_bits - 1; bit >= 0; bit-- ) {
    ret |= (get_bit() << bit);
  }

  return ret;
}

int32_t BoolDecoder::get_int( const unsigned int num_bits )
{
  uint32_t ret = get_uint( num_bits );
  return get_bit() ? -ret : ret;
}

int32_t BoolDecoder::maybe_get_int( const unsigned int num_bits )
{
  return get_bit() ? get_int( num_bits ) : 0;
}
