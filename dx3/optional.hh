#ifndef OPTIONAL_HH
#define OPTIONAL_HH

#include <functional>
#include <cassert>

/* Simple class for default-constructible initialize-once optional types */

template <class T>
class Optional
{
private:
  bool initialized_;
  union { T object_; bool missing_; };

public:
  Optional() : initialized_( false ), missing_( true ) {}
  Optional( const T & other ) : initialized_( true ), object_( other ) {}

  bool initialized( void ) const { return initialized_; }
  const T * operator->() const { assert( initialized() ); return &object_; }
};


#endif /* OPTIONAL_HH */
