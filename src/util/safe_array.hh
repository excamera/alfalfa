#ifndef SAFE_ARRAY_HH
#define SAFE_ARRAY_HH

#include <cassert>

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
};

#endif /* ARRAY_HH */
