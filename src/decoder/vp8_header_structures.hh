#ifndef VP8_HEADER_STRUCTURES_HH
#define VP8_HEADER_STRUCTURES_HH

#include <vector>
#include <type_traits>

#include "optional.hh"
#include "bool_decoder.hh"
#include "vp8_prob_data.hh"

class Boolean
{
private:
  bool i_;

public:
  Boolean( BoolDecoder & data, const Probability probability = 128 ) : i_( data.get( probability ) ) {}
  Boolean( const bool & val ) : i_( val ) {}
  operator const bool & () const { return i_; }
};

template <unsigned int width>
class Unsigned
{
private:
  uint8_t i_;

public:
  Unsigned( BoolDecoder & data ) : i_( (Boolean( data ) << (width-1)) | Unsigned<width-1>( data ) )
  {
    static_assert( width <= 8, "Unsigned width must be <= 8" );
  }
  Unsigned( const uint8_t & val ) : i_( val ) {}

  operator const uint8_t & () const { return i_; }
};

template <>
inline Unsigned< 0 >::Unsigned( BoolDecoder & ) : i_() {}

template <unsigned int width>
class Signed
{
private:
  int8_t i_;

public:
  Signed( BoolDecoder & data ) : i_( Unsigned<width>( data ) * (Boolean( data ) ? -1 : 1) )
  {
    static_assert( width <= 7, "Signed width must be <= 7" );
  }
  Signed( const int8_t & val ) : i_( val ) {}

  operator const int8_t & () const { return i_; }
};

class Flag
{
  bool i_;

public:
  Flag( BoolDecoder & data ) : i_( Boolean( data ) ) {}
  Flag( const bool & val ) : i_( val ) {}
  operator const bool & () const { return i_; }
};

template <class T>
class Flagged
{
private:
  Optional< T > storage_;

public:
  Flagged( BoolDecoder & data, const Probability probability = 128 )
    : storage_( Boolean( data, probability ), data )
  {}

  bool initialized( void ) const { return storage_.initialized(); }
  const T & get( void ) const { return storage_.get(); }
  const T & get_or( const T & default_value ) const { return storage_.get_or( default_value ); }
  operator const Optional<T> & () const { return storage_; }
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

  const T & at( const unsigned int offset ) const
  {
    #ifdef NDEBUG
    return storage_[ offset ];
    #else
    return storage_.at( offset );
    #endif
  }

  static constexpr unsigned int size( void ) { return len; }

  virtual ~Array() {}
};

template <class T, unsigned int size>
class Enumerate : public Array< T, size >
{
public:
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

template <class enumeration, uint8_t alphabet_size, const TreeArray< alphabet_size > & nodes>
class Tree
{
private:
  enumeration value_;

public:
  typedef enumeration type;
  typedef const ProbabilityArray< alphabet_size > probability_array_type;

  Tree( BoolDecoder & data, probability_array_type & probabilities )
    : value_( data.tree< alphabet_size, enumeration >( nodes, probabilities ) )
  {}

  Tree( const enumeration & x ) : value_( x ) {}

  operator const enumeration & () const { return value_; }
};

class MotionVector
{
private:
  int16_t y_, x_; /* y is sent before x in the bitstream */

  static int16_t read_component( BoolDecoder & data,
				 const SafeArray< Probability, MV_PROB_CNT > & component_probs );

public:
  MotionVector() : y_(), x_() {}
  MotionVector( const int16_t x, const int16_t y ) : y_( y ), x_( x ) {};

  const int16_t & x( void ) const { return x_; }
  const int16_t & y( void ) const { return y_; }

  bool operator==( const MotionVector & other ) const { return x_ == other.x_ and y_ == other.y_; }
  void operator+=( const MotionVector & other ) { x_ += other.x_; y_ += other.y_; }

  MotionVector operator-() const { return MotionVector( -x(), -y() ); }

  MotionVector( BoolDecoder & data,
		const SafeArray< SafeArray< Probability, MV_PROB_CNT >, 2 > & motion_vector_probs );

  void clamp( const int16_t to_left, const int16_t to_right,
	      const int16_t to_top, const int16_t to_bottom );

  bool empty( void ) const { return x_ == 0 and y_ == 0; }
};

#endif /* VP8_HEADER_STRUCTURES_HH */
