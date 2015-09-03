#include "decoder_tracking.hh"

#include <sstream>
#include <string>
#include <boost/functional/hash.hpp>

#include "exception.hh"

using namespace std;

bool & DependencyTracker::reference( const reference_frame reference_id )
{
  switch( reference_id ) {
    case LAST_FRAME: 
      return need_last;
    case GOLDEN_FRAME: 
      return need_golden;
    case ALTREF_FRAME:
      return need_alternate;
    default: throw LogicError();
  }
}

// Helper functions to decode frame name
static SourceHash decode_source( const string & frame_name )
{
  SafeArray<Optional<size_t>, 5> components;

  size_t end_pos = frame_name.find( '#' );
  
  size_t split_pos = 0;
  for ( int i = 0; i < 5; i++ ) {
    size_t old_pos = split_pos;

    split_pos = frame_name.find( '_', old_pos );

    if ( split_pos == string::npos or split_pos >= end_pos ) {
      // Find the next '#', otherwise we would include the whole remainder of the string
      split_pos = end_pos;
    }
    
    string component = string( frame_name, old_pos, split_pos - old_pos );
    if ( component == "x" ) {
      components.at( i ) = Optional<size_t>();
    }
    else {
      components.at( i ) = Optional<size_t>( stoul( component, nullptr, 16 ) );
    }

    split_pos++;
  }

  return SourceHash( components.at( 0 ), components.at( 1 ), components.at( 2 ),
                     components.at( 3 ), components.at( 4 ) );
}

SourceHash::SourceHash( const string & frame_name )
  : SourceHash( decode_source( frame_name ) )
{}

template<bool state_init, bool continuation_init, bool last_init, bool golden_init, bool alternate_init>
bool generic_can_decode( const SourceHash & source, const DecoderHash & decoder )
{
  return ( !state_init or source.state_hash.get() == decoder.state_hash() ) and
    ( !continuation_init or source.continuation_hash.get() == decoder.continuation_hash() ) and 
    ( !last_init or source.last_hash.get() == decoder.last_hash() ) and
    ( !golden_init or source.golden_hash.get() == decoder.golden_hash() ) and 
    ( !alternate_init or source.alt_hash.get() == decoder.alt_hash() );
}

SourceHash::SourceHash( const Optional<size_t> & state, const Optional<size_t> & continuation,
                        const Optional<size_t> & last, const Optional<size_t> & golden,
                        const Optional<size_t> & alt )
  : state_hash( state ),
    continuation_hash( continuation ),
    last_hash( last ),
    golden_hash( golden ),
    alt_hash( alt ),
    check( nullptr )
{
  unsigned idx = 0;
  if ( state_hash.initialized() ) {
    idx |= 1;
  }
  if ( continuation_hash.initialized() ) {
    idx |= 2;
  }
  if ( last_hash.initialized() ) {
    idx |= 4;
  }
  if ( golden_hash.initialized() ) {
    idx |= 8;
  }
  if ( alt_hash.initialized() ) {
    idx |= 16;
  }

  CheckFunc func_lookup[ 32 ] = {
    &generic_can_decode<false, false, false, false, false>, &generic_can_decode<true, false, false, false, false>,
    &generic_can_decode<false, true, false, false, false>, &generic_can_decode<true, true, false, false, false>,
    &generic_can_decode<false, false, true, false, false>, &generic_can_decode<true, false, true, false, false>,
    &generic_can_decode<false, true, true, false, false>, &generic_can_decode<true, true, true, false, false>,
    &generic_can_decode<false, false, false, true, false>, &generic_can_decode<true, false, false, true, false>,
    &generic_can_decode<false, true, false, true, false>, &generic_can_decode<true, true, false, true, false>,
    &generic_can_decode<false, false, true, true, false>, &generic_can_decode<true, false, true, true, false>,
    &generic_can_decode<false, true, true, true, false>, &generic_can_decode<true, true, true, true, false>,
    &generic_can_decode<false, false, false, false, true>, &generic_can_decode<true, false, false, false, true>,
    &generic_can_decode<false, true, false, false, true>, &generic_can_decode<true, true, false, false, true>,
    &generic_can_decode<false, false, true, false, true>, &generic_can_decode<true, false, true, false, true>,
    &generic_can_decode<false, true, true, false, true>, &generic_can_decode<true, true, true, false, true>,
    &generic_can_decode<false, false, false, true, true>, &generic_can_decode<true, false, false, true, true>,
    &generic_can_decode<false, true, false, true, true>, &generic_can_decode<true, true, false, true, true>,
    &generic_can_decode<false, false, true, true, true>, &generic_can_decode<true, false, true, true, true>,
    &generic_can_decode<false, true, true, true, true>, &generic_can_decode<true, true, true, true, true>
  };

  check = func_lookup[ idx ];
}

static ostream& operator<<( ostream & out, const Optional<size_t> & hash )
{
  if ( hash.initialized() ) {
    return out << hash.get();
  }
  else {
    return out << "x";
  }
}

string SourceHash::str( void ) const
{
  stringstream hash_str;
  hash_str << hex << uppercase;

  hash_str << state_hash << "_" << continuation_hash << "_" <<
    last_hash << "_" << golden_hash << "_" << alt_hash;

  return hash_str.str();
}

UpdateTracker::UpdateTracker( bool set_update_last, bool set_update_golden,
                              bool set_update_alternate, bool set_last_to_golden,
                              bool set_last_to_alternate, bool set_golden_to_alternate,
                              bool set_alternate_to_golden )
  : update_last( set_update_last ),
    update_golden( set_update_golden ),
    update_alternate( set_update_alternate ),
    last_to_golden( set_last_to_golden ),
    last_to_alternate( set_last_to_alternate ),
    golden_to_alternate( set_golden_to_alternate ),
    alternate_to_golden( set_alternate_to_golden )
{}

static TargetHash decode_target( const string & frame_name )
{
  SafeArray<size_t, 11> components;
  
  size_t split_pos = frame_name.find( '#' );

  for ( int i = 0; i < 11; i++ ) {
    size_t old_pos = split_pos + 1;

    split_pos = frame_name.find( '_', old_pos );

    string component = string( frame_name, old_pos, split_pos - old_pos );
    components.at( i ) = stoul( component, nullptr, 16 );
  }

  return TargetHash( UpdateTracker( components.at( 4 ), components.at ( 5 ), components.at( 6 ),
                                    components.at( 7 ), components.at( 8 ), components.at( 9 ),
                                    components.at( 10 ) ),
                     components.at( 0 ), components.at( 1 ), components.at( 2 ), components.at( 3 ) ); 
}

TargetHash::TargetHash( const string & frame_name )
  : TargetHash( decode_target( frame_name ) )
{}

TargetHash::TargetHash( const UpdateTracker & updates, size_t state,
                        size_t continuation, size_t output, bool show )
  : UpdateTracker( updates ),
    state_hash( state ),
    continuation_hash( continuation ),
    output_hash( output ),
    shown( show )
{}

string TargetHash::str() const
{
  stringstream hash_str;
  hash_str << hex << uppercase;

  hash_str << state_hash << "_" << continuation_hash << "_" <<
    output_hash << "_" << shown << "_" << update_last << "_" <<
    update_golden << "_" << update_alternate <<
    "_" << last_to_golden << "_" << last_to_alternate << "_" <<
    golden_to_alternate << "_" << alternate_to_golden;

  return hash_str.str();
}

DecoderHash::DecoderHash( size_t state_hash, size_t continuation_hash, size_t last_hash, size_t golden_hash, size_t alt_hash )
  : state_hash_( state_hash ),
    continuation_hash_( continuation_hash ),
    last_hash_( last_hash ),
    golden_hash_( golden_hash ),
    alt_hash_( alt_hash )
{}

bool DecoderHash::can_decode( const SourceHash & source_hash ) const
{
  return source_hash.check( source_hash, *this );
}

void DecoderHash::update( const TargetHash & target_hash )
{
  state_hash_ = target_hash.state_hash;

  continuation_hash_ = target_hash.continuation_hash;

  if ( target_hash.last_to_alternate ) {
    alt_hash_ = last_hash_;
  }
  else if ( target_hash.golden_to_alternate ) {
    alt_hash_ = golden_hash_;
  }

  if ( target_hash.last_to_golden ) {
    golden_hash_ = last_hash_;
  }
  else if ( target_hash.alternate_to_golden ) {
    golden_hash_ = alt_hash_;
  }

  if ( target_hash.update_last ) {
    last_hash_ = target_hash.output_hash;
  }

  if ( target_hash.update_golden ) {
    golden_hash_ = target_hash.output_hash;
  }

  if ( target_hash.update_alternate ) {
    alt_hash_ = target_hash.output_hash;
  }
}

size_t DecoderHash::state_hash( void ) const
{
  return state_hash_;
}

size_t DecoderHash::continuation_hash( void ) const
{
  return continuation_hash_;
}

size_t DecoderHash::last_hash( void ) const
{
  return last_hash_;
}

size_t DecoderHash::golden_hash( void ) const
{
  return golden_hash_;
}

size_t DecoderHash::alt_hash( void ) const
{
  return alt_hash_;
}

size_t DecoderHash::hash( void ) const
{
  size_t hash_val = 0;
  boost::hash_combine( hash_val, state_hash_ );
  boost::hash_combine( hash_val, continuation_hash_ );
  boost::hash_combine( hash_val, last_hash_ );
  boost::hash_combine( hash_val, golden_hash_ );
  boost::hash_combine( hash_val, alt_hash_ );
  return hash_val;
}

string DecoderHash::str( void ) const
{
  stringstream hash_str;
  hash_str << hex << uppercase;

  hash_str << state_hash_ << "_" << continuation_hash_ << "_" <<
    last_hash_ << "_" << golden_hash_ << "_" << alt_hash_;

  return hash_str.str();
}

bool DecoderHash::operator==( const DecoderHash & other ) const
{
  return state_hash_ == other.state_hash_ and
         continuation_hash_ == other.continuation_hash_ and
         last_hash_ == other.last_hash_ and
         golden_hash_ == other.golden_hash_ and
         alt_hash_ == other.alt_hash_;
}
