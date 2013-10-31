#ifndef VP8_HEADER_STRUCTURES_HH
#define VP8_HEADER_STRUCTURES_HH

#include <vector>

#include "optional.hh"

template <class T>
class Flagged : public Optional<T>
{
public:
  Flagged( BoolDecoder & data )
    : Optional<T>( data.bit() ? T( data ) : Optional<T>() )
  {}
};

template <int width>
class Unsigned
{
private:
  uint8_t i_;

public:
  Unsigned( BoolDecoder & data ) : i_( data.uint( width ) )
  {
    static_assert( width <= 8, "Unsigned width must be <= 8" );
  }
  operator const uint8_t & () const { return i_; }
};

template <int width>
class Signed
{
private:
  int8_t i_;

public:
  Signed( BoolDecoder & data ) : i_( data.sint( width ) )
  {
    static_assert( width <= 7, "Signed width must be <= 7" );
  }
  operator const int8_t & () const { return i_; }
};

class Bool
{
private:
  bool i_;

public:
  Bool( BoolDecoder & data ) : i_( data.bit() ) {}
  operator const bool & () const { return i_; }
};

template <class T, unsigned int size>
class Array
{
private:
  std::vector< T > storage_;

public:
  Array( BoolDecoder & data )
  : storage_()
  {
    storage_.reserve( size );
    for ( unsigned int i = 0; i < size; i++ ) {
      storage_.emplace_back( data );
    }
  }

  const T & at( const typename decltype( storage_ )::size_type & offset ) const
  {
    return storage_.at( offset );
  }
};

#endif /* VP8_HEADER_STRUCTURES_HH */
