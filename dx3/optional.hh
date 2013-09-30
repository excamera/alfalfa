#ifndef OPTIONAL_HH
#define OPTIONAL_HH

#include <functional>

/* Simple class for default-constructible initialize-once optional types */

template <class T>
class Optional
{
private:
  bool initialized_;
  union { T object_; };

public:
  Optional() : initialized_( false ) {}
  Optional( const T & other ) : initialized_( true ), object_( other ) {}

  bool initialized( void ) const { return initialized_; }
  const T & operator() (const T &) const { assert( initialized() ); return object_; }
};

#endif /* OPTIONAL_HH */
