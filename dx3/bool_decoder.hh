#ifndef BOOL_DECODER_HH
#define BOOL_DECODER_HH

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
  bool get_bit( void ) { return get( 128 ); }
  uint32_t get_uint( const unsigned int num_bits );
  int32_t get_int( const unsigned int num_bits );
  int32_t maybe_get_int( const unsigned int num_bits );
};

#endif /* BOOL_DECODER_HH */
