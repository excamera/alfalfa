#ifndef BOOL_ENCODER_HH
#define BOOL_ENCODER_HH

#include <vector>

#include "bool_decoder.hh"

class BoolEncoder
{
private:
  std::vector< uint8_t > output_ {};

  uint32_t range_ { 255 }, bottom_ { 0 };
  char bit_count_ { 24 };

  void add_one_to_output( void )
  {
    auto it = output_.end();
    while ( *--it == 255 ) {
      *it = 0;
      assert( it != output_.begin() );
    }
    ++*it;
  }

  void flush( void )
  {
    int c = bit_count_;
    uint32_t v = bottom_;

    if ( v & (1 << (32 - c)) ) {  /* propagate (unlikely) carry */
      add_one_to_output();
    }

    v <<= c & 7;               /* before shifting remaining output */
    c >>= 3;                   /* to top of internal buffer */
    while (--c >= 0) {
      v <<= 8;
    }
    c = 4;
    while (--c >= 0) {    /* write remaining data, possibly padded */
      output_.emplace_back( v >> 24 );
      v <<= 8;
    }
  }

public:
  BoolEncoder() {}

  void put( const Probability probability, const bool value )
  {
    uint32_t split = 1 + (((range_ - 1) * probability) >> 8);

    if ( value ) {
      bottom_ += split; /* move up bottom of interval */
      range_ -= split;  /* with corresponding decrease in range */
    } else {
      range_ = split;   /* decrease range, leaving bottom alone */
    }

    while ( range_ < 128 ) {
      range_ <<= 1;

      if ( bottom_ & (1 << 31) ) { /* detect carry */
	add_one_to_output();
      }

      bottom_ <<= 1;        /* before shifting bottom */

      if ( ! --bit_count_ ) {  /* write out high byte of bottom ... */
	output_.emplace_back( bottom_ >> 24 );
	bottom_ &= (1 << 24) - 1;  /* ... keeping low 3 bytes */
	bit_count_ = 8;            /* 8 shifts until next output */
      }
    }
  }

  std::vector< uint8_t > finish( void )
  {
    flush();
    std::vector< uint8_t > ret( move( output_ ) );
    *this = BoolEncoder();
    return ret;
  }
};

#endif /* BOOL_ENCODER_HH */
