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

  Optional( const Optional<T> & other ) : initialized_( other.initialized_ ), object_( other.object_ ) {}

  ~Optional() { if ( initialized() ) { object_.~T(); } }
};


#endif /* OPTIONAL_HH */
