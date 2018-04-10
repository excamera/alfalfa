/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Copyright 2013-2018 the Alfalfa authors
                       and the Massachusetts Institute of Technology

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

      1. Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

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
  Boolean( const bool & val = false ) : i_( val ) {}
  operator const bool & () const { return i_; }
};

using Flag = Boolean;

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
  Unsigned( const uint8_t & val = 0 ) : i_( val ) {}

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

template <class T>
class Flagged : public Optional<T>
{
public:
  Flagged( BoolDecoder & data, const Probability probability = 128 )
    : Optional<T>( Flag( data, probability ), data )
  {}

  using Optional<T>::Optional;
  Flagged() = default;
};

/* An Array of VP8 header elements.
   A header element may optionally take its position in the array as an argument. */

template <class T, unsigned int len>
class Array
{
protected:
  std::vector< T > storage_;

  Array( const bool ) : storage_() {}

public:
  Array() : storage_( len, T() ) {}

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

  T & at( const unsigned int offset )
  {
    #ifdef NDEBUG
    return storage_[ offset ];
    #else
    return storage_.at( offset );
    #endif
  }

  static constexpr unsigned int size( void ) { return len; }

  bool operator==( const Array & other ) const { return storage_ == other.storage_; }
};

template <class T, unsigned int size>
class Enumerate : public Array< T, size >
{
public:
  Enumerate() : Array<T,size>() {}

  template < typename... Targs >
  Enumerate( BoolDecoder & data, Targs&&... Fargs )
    : Array<T,size>( false )
  {
    Array<T, size>::storage_.reserve( size );
    for ( unsigned int i = 0; i < size; i++ ) {
      Array<T, size>::storage_.emplace_back( data, i, std::forward<Targs>( Fargs )... );
    }
  }
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
  void operator-=( const MotionVector & other ) { *this += -other; }

  MotionVector operator+( const MotionVector & other ) const
  {
    MotionVector res( *this );
    res += other;
    return res;
  }

  MotionVector operator-( const MotionVector & other ) const
  {
    MotionVector res( *this );
    res -= other;
    return res;
  }

  MotionVector operator-() const { return MotionVector( -x(), -y() ); }

  MotionVector( BoolDecoder & data,
                const SafeArray< SafeArray< Probability, MV_PROB_CNT >, 2 > & motion_vector_probs );

  void clamp( const int16_t to_left, const int16_t to_right,
              const int16_t to_top, const int16_t to_bottom );

  bool empty( void ) const { return x_ == 0 and y_ == 0; }

  static MotionVector luma_to_chroma( const MotionVector & s1,
                                      const MotionVector & s2,
                                      const MotionVector & s3,
                                      const MotionVector & s4 );
};

#endif /* VP8_HEADER_STRUCTURES_HH */
