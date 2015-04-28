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

  static constexpr unsigned int size( void ) { return size_param; }

  template <unsigned int offset, unsigned int len>
  const SafeArray<T, len> & slice( void ) const
  {
    static_assert( offset + len < size_param, "SafeArray slice extends past end of array" );
    return *reinterpret_cast< const SafeArray<T, len> *>( storage_ + offset );
  }

  const T & last( void ) const { return storage_[ size_param - 1 ]; }

  bool operator==( const SafeArray<T, size_param> & other ) const
  {
    return 0 == memcmp( storage_, other.storage_, size_param * sizeof( T ) );
  }
};

#endif /* ARRAY_HH */
