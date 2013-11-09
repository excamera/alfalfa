#ifndef OPTIONAL_HH
#define OPTIONAL_HH

#include <functional>
#include <cassert>
#include <utility>

template <class T>
class Optional
{
private:
  bool initialized_;
  union { T object_; bool missing_; };

public:
  Optional() : initialized_( false ), missing_( true ) {}
  Optional( T && other ) : initialized_( true ), object_( std::move( other ) ) {}

  template <typename... Targs>
  Optional( const bool is_present, Targs&&... Fargs )
    : Optional( is_present ? Optional( T( std::forward<Targs>( Fargs )... ) ) : Optional() )
  {}

  bool initialized( void ) const { return initialized_; }
  const T & get( void ) const { assert( initialized() ); return object_; }
  const T & get_or( const T & default_value ) const { return initialized() ? object_ : default_value; }

  Optional( const Optional<T> & other ) : initialized_( other.initialized_ ), object_( other.object_ ) {}
  Optional<T> & operator=( const Optional<T> & other )
  {
    initialized_ = other.initialized_;
    object_ = other.object_;
    return *this;
  }

  virtual ~Optional() { if ( initialized() ) { object_.~T(); } }
};


#endif /* OPTIONAL_HH */
