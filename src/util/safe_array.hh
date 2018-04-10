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

#ifndef SAFE_ARRAY_HH
#define SAFE_ARRAY_HH

#include <cassert>
#include <cstring>

/* Just like std::array, but with safety controllable by NDEBUG macro */

template <class T, unsigned int size_param>
struct SafeArray
{
  T storage_[ size_param ];

  inline T & at( const unsigned int index )
  {
    assert( index < size() );
    return storage_[ index ];
  }

  inline const T & at( const unsigned int index ) const
  {
    assert( index < size() );
    return storage_[ index ];
  }

  /* Probably want to reimplement these with a proper iterator */
  const T * begin( void ) const
  {
    return &storage_[0];
  }

  const T * end( void ) const
  {
    return &storage_[ size_param ];
  }

  static constexpr unsigned int size( void ) { return size_param; }

  template <unsigned int offset, unsigned int len>
  const SafeArray<T, len> & slice( void ) const
  {
    static_assert( offset + len <= size_param, "SafeArray slice extends past end of array" );
    return *reinterpret_cast<const SafeArray<T, len> *>( storage_ + offset );
  }

  template <unsigned int offset, unsigned int len>
  SafeArray<T, len> & slice( void )
  {
    static_assert( offset + len <= size_param, "SafeArray slice extends past end of array" );
    return *reinterpret_cast<SafeArray<T, len> *>( storage_ + offset );
  }

  const T & last( void ) const { return storage_[ size_param - 1 ]; }

  bool operator==( const SafeArray<T, size_param> & other ) const
  {
    return 0 == memcmp( storage_, other.storage_, size_param * sizeof( T ) );
  }

  bool operator!=( const SafeArray<T, size_param> & other ) const
  {
    return not operator==( other );
  }
};

#endif /* ARRAY_HH */
