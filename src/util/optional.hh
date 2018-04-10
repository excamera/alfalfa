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

#ifndef OPTIONAL_HH
#define OPTIONAL_HH

#include <stdexcept>
#include <cassert>
#include <utility>

template <class T>
class Optional
{
private:
  bool initialized_;
  union { T object_; bool missing_; };

public:
  /* constructor for uninitialized optional */
  /* missing_ field of union is necessary to get rid of "may be used uninitialized" warning */
  Optional() : initialized_( false ), missing_() {}

  /* constructor for initialized optional */
  Optional( T && other ) : initialized_( true ), object_( std::move( other ) ) {}

  /* conditional constructor */
  // FIXME, this leads to very misleading results when constructing optional ints
  template <typename... Targs>
  Optional( const bool is_present, Targs&&... Fargs )
    : initialized_( is_present )
  {
    if ( initialized_ ) {
      new( &object_ ) T( std::forward<Targs>( Fargs )... );
    }
  }

  /* move constructor */
  Optional( Optional<T> && other ) noexcept( std::is_nothrow_move_constructible<T>::value )
    : initialized_( other.initialized_ )
  {
    if ( initialized_ ) {
      new( &object_ ) T( std::move( other.object_ ) );
    }
  }

  /* copy constructor */
  Optional( const Optional<T> & other )
    : initialized_( other.initialized_ )
  {
    if ( initialized_ ) {
      new( &object_ ) T( other.object_ );
    }
  }

  /* initialize in place */
  template <typename... Targs>
  void initialize( Targs&&... Fargs )
  {
    assert( not initialized() );
    new( &object_ ) T( std::forward<Targs>( Fargs )... );
    initialized_ = true;
  }

  /* move assignment operators */
  const Optional & operator=( Optional<T> && other )
  {
    /* destroy if necessary */
    if ( initialized_ ) {
      object_.~T();
    }

    initialized_ = other.initialized_;
    if ( initialized_ ) {
      new( &object_ ) T( std::move( other.object_ ) );
    }
    return *this;
  }

  /* copy assignment operator */
  const Optional & operator=( const Optional<T> & other )
  {
    /* destroy if necessary */
    if ( initialized_ ) {
      object_.~T();
    }

    initialized_ = other.initialized_;
    if ( initialized_ ) {
      new( &object_ ) T( other.object_ );
    }
    return *this;
  }

  /* equality */
  bool operator==( const Optional<T> & other ) const
  {
    if ( initialized_ ) {
      return other.initialized_ ? (get() == other.get()) : false;
    } else {
      return !other.initialized_;
    }
  }

  bool operator!=( const Optional<T> & other ) const
  {
    return not operator==( other );
  }

  /* getters */
  bool initialized( void ) const { return initialized_; }
  const T & get( void ) const { assert( initialized() ); return object_; }
  const T & get_or( const T & default_value ) const { return initialized() ? object_ : default_value; }

  T & get( void ) { assert( initialized() ); return object_; }

  /* destructor */
  ~Optional() { if ( initialized() ) { object_.~T(); } }

  void clear( void ) { if ( initialized() ) { object_.~T(); } initialized_ = false; }

  template <typename... Targs>
  void reset( Targs&&... Fargs ) { clear(); initialize( Fargs... ); }
};

template <class T>
Optional<T> make_optional( bool initialized, const T & val )
{
  return Optional<T>( initialized, val );
}

#endif /* OPTIONAL_HH */
