#ifndef REFERENCE_TRACKER_HH
#define REFERENCE_TRACKER_HH

class ReferenceTracker
{
private:
  bool continuation_, last_, golden_, alternate_;

public:
  ReferenceTracker( bool continuation, bool last, bool golden,
                    bool alternate )
    : continuation_( continuation ), last_( last ),
      golden_( golden ), alternate_( alternate )
  {}

  bool continuation( void ) const { return continuation_; }
  void set_continuation( bool continuation ) { continuation_ = continuation; }

  bool last( void ) const { return last_; }
  void set_last( bool last ) { last_ = last; }

  bool golden( void ) const { return golden_; }
  void set_golden( bool golden ) { golden_ = golden; }

  bool alternate( void ) const { return alternate_; }
  void set_alternate( bool alternate ) { alternate_ = alternate; }

  bool & operator[]( const reference_frame reference_id )
  {
    switch( reference_id ) {
      case LAST_FRAME: return last_;
      case GOLDEN_FRAME: return golden_;
      case ALTREF_FRAME: return alternate_;
      default: throw LogicError();
    }
  }

  bool operator==( const ReferenceTracker & other ) const
  {
    return continuation_ == other.continuation_ and last_ == other.last_ and
      golden_ == other.golden_ and alternate_ == other.alternate_;
  }
};

#endif
