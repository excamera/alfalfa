#ifndef VP8_HEADER_STRUCTURES_HH
#define VP8_HEADER_STRUCTURES_HH

#include <vector>
#include <type_traits>

#include "optional.hh"
#include "bool_decoder.hh"

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
  Unsigned( const uint8_t & val ) : i_( val ) {}

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
  Signed( const int8_t & val ) : i_( val ) {}

  operator const int8_t & () const { return i_; }
};

class Bool
{
private:
  bool i_;

public:
  Bool( BoolDecoder & data, const Probability probability ) : i_( data.get( probability ) ) {}
  Bool( const bool & val ) : i_( val ) {}
  operator const bool & () const { return i_; }
  virtual ~Bool() {}
};

class Flag : public Bool
{
public:
  Flag( BoolDecoder & data ) : Bool( data, 128 ) {}
};

template <class T>
class Flagged : public Optional<T>
{
public:
  Flagged( BoolDecoder & data, const Probability probability = 128 )
    : Optional<T>( Bool( data, probability ), data )
  {}
};

/* An Array of VP8 header elements.
   A header element may optionally take its position in the array as an argument. */

template <class T, unsigned int len>
class Array
{
protected:
  std::vector< T > storage_;

  Array() : storage_() {}

public:
  template < typename... Targs >
  Array( BoolDecoder & data, Targs&&... Fargs )
    : storage_()
  {
    storage_.reserve( len );
    for ( unsigned int i = 0; i < len; i++ ) {
      storage_.emplace_back( data, std::forward<Targs>( Fargs )... );
    }
  }

  Array( const std::array< T, len > & other )
    : storage_( begin( other ), end( other ) )
  {}

  Array( const std::vector< T > & other )
    : storage_( other )
  {}

  Array( const std::vector< T > && other )
    : storage_( move( other ) )
  {}

  const T & at( const typename decltype( storage_ )::size_type & offset ) const
  {
    return storage_.at( offset );
  }

  operator const std::array< T, len > & () const
  {
    return *reinterpret_cast< const std::array< T, len > * >( &storage_.at( 0 ) );
  }

  size_t size( void ) const { return len; }

  virtual ~Array() {}
};

template <class T, unsigned int size>
class Enumerate : public Array< T, size >
{
public:
  using Array< T, size >::Array;

  template < typename... Targs >
  Enumerate( BoolDecoder & data, Targs&&... Fargs )
    : Array<T, size>()
  {
    Array<T, size>::storage_.reserve( size );
    for ( unsigned int i = 0; i < size; i++ ) {
      Array<T, size>::storage_.emplace_back( data, i, std::forward<Targs>( Fargs )... );
    }
  }
};

template <class T, unsigned int size>
class EnumerateContext : public Array< T, size >
{
public:
  using Array< T, size >::Array;

  template < typename... Targs >
  EnumerateContext( BoolDecoder & data, Targs&&... Fargs )
    : Array<T, size>()
  {
    Array<T, size>::storage_.reserve( size );
    for ( unsigned int i = 0; i < size; i++ ) {
      Array<T, size>::storage_.emplace_back( data, i, *const_cast< const EnumerateContext<T, size> * >( this ), std::forward<Targs>( Fargs )... );
    }
  }
};

template <class enumeration, uint8_t alphabet_size, const TreeArray< alphabet_size > & nodes>
class Tree
{
private:
  enumeration value_;

public:
  Tree( BoolDecoder & data, const ProbabilityArray< alphabet_size > & probabilities )
    : value_( data.tree< alphabet_size, enumeration >( nodes, probabilities ) )
  {}

  Tree( const enumeration & x ) : value_( x ) {}

  operator const enumeration & () const { return value_; }

  virtual ~Tree() {}
};

#endif /* VP8_HEADER_STRUCTURES_HH */
