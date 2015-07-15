#ifndef DEPENDENCY_TRACKER_HH
#define DEPENDENCY_TRACKER_HH

#include "modemv_data.hh"
#include "exception.hh"

class DependencyTracker
{
private:
  bool state_, continuation_, last_, golden_, alternate_;

public:
  DependencyTracker( bool state, bool continuation, bool last, bool golden,
                     bool alternate )
    : state_( state ), continuation_( continuation ), last_( last ),
      golden_( golden ), alternate_( alternate )
  {}

  bool state( void ) const { return state_; }
  void set_state( bool state ) { state_ = state; }

  bool continuation( void ) const { return continuation_; }
  void set_continuation( bool continuation ) { continuation_ = continuation; }

  bool last( void ) const { return last_; }
  void set_last( bool last ) { last_ = last; }

  bool golden( void ) const { return golden_; }
  void set_golden( bool golden ) { golden_ = golden; }

  bool alternate( void ) const { return alternate_; }
  void set_alternate( bool alternate ) { alternate_ = alternate; }

  void set_reference( const reference_frame reference_id, bool b )
  {
    switch( reference_id ) {
      case LAST_FRAME: 
        last_ = b;
        break;
      case GOLDEN_FRAME: 
        golden_ = b;
        break;
      case ALTREF_FRAME:
        alternate_ = b;
        break;
      default: throw LogicError();
    }
  }

  bool operator==( const DependencyTracker & other ) const
  {
    return state_ == other.state_ and continuation_ == other.continuation_ and 
      last_ == other.last_ and golden_ == other.golden_ and alternate_ == other.alternate_;
  }
};

#endif
