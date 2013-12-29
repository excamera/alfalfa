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
};

#endif /* ARRAY_HH */
