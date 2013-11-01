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
  typedef T object_type;

  Optional() : initialized_( false ), missing_( true ) {}
  Optional( const T & other ) : initialized_( true ), object_( other ) {}

  bool initialized( void ) const { return initialized_; }
  const T & get() const { assert( initialized() ); return object_; }
  const T & get_or( const T & default_value ) const { return initialized() ? object_ : default_value; }

  Optional( const Optional<T> & other ) : initialized_( other.initialized_ ), object_( other.object_ ) {}

  virtual ~Optional() { if ( initialized() ) { object_.~T(); } }
};


#endif /* OPTIONAL_HH */
