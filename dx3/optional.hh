#ifndef OPTIONAL_HH
#define OPTIONAL_HH

#include <functional>

/* Simple class for default-constructible initialize-once optional types */

template <class T>
class Optional
{
private:
  bool initialized_;
  T object_;

public:
  Optional( bool s_initialized, std::function<T(void)> other_func )
    : initialized_( s_initialized ),
      object_( initialized_ ? other_func() : T() )
  {}

  bool initialized( void ) const { return initialized_; }
  const T & operator() (const T &) const { assert( initialized() ); return object_; }
};

#endif /* OPTIONAL_HH */
