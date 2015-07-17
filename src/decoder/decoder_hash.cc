#include "decoder_hash.hh"

#include <sstream>
#include <string>
#include <boost/functional/hash.hpp>

using namespace std;

// Helper functions to decode frame name
static SafeArray<Optional<size_t>, 5> split( const string & frame_name, size_t split_pos )
{
  SafeArray<Optional<size_t>, 5> components;

  size_t end_pos = frame_name.find( '#', split_pos + 1 );
  
  for ( int i = 0; i < 5; i++ ) {
    size_t old_pos = split_pos + 1;

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
  }

  return components;
}

static SafeArray<Optional<size_t>, 5> split_target( const string & frame_name )
{
  return split( frame_name, frame_name.find( '#' ) );
}

static SafeArray<Optional<size_t>, 5> split_source( const string & frame_name )
{
  return split( frame_name, -1 );
}

DecoderHash::DecoderHash( const string & frame_name, bool target )
  : hashes_( target ? split_target( frame_name ) : split_source( frame_name ) )
{}

DecoderHash::DecoderHash( Optional<size_t> state_hash, Optional<size_t> continuation_hash,
                          Optional<size_t> last_hash, Optional<size_t> golden_hash,
                          Optional<size_t> alt_hash )
  : hashes_ { state_hash, continuation_hash, last_hash, golden_hash, alt_hash }
{}

DecoderHash::DecoderHash( size_t state_hash, size_t continuation_hash, size_t last_hash,
                          size_t golden_hash, size_t alt_hash )
  : DecoderHash( Optional<size_t>( true, state_hash ), Optional<size_t>( true, continuation_hash ),
                 Optional<size_t>( true, last_hash ), Optional<size_t>( true, golden_hash ), Optional<size_t>( true, alt_hash ) )
{}

DecoderHash DecoderHash::filter( const DependencyTracker & tracker ) const
{
  DecoderHash new_hash = *this;

  if ( not tracker.state() ) {
    new_hash.hashes_.at( 0 ).clear();
  }
  if ( not tracker.continuation() ) {
    new_hash.hashes_.at( 1 ).clear();
  }
  if ( not tracker.last() ) {
    new_hash.hashes_.at( 2 ).clear();
  }
  if ( not tracker.golden() ) {
    new_hash.hashes_.at( 3 ).clear();
  }
  if ( not tracker.alternate() ) {
    new_hash.hashes_.at( 4 ).clear();
  }

  return new_hash;
}

Optional<size_t> DecoderHash::state( void ) const
{
  return hashes_.at( 0 );
}

Optional<size_t> DecoderHash::continuation( void ) const
{
  return hashes_.at( 1 );
}

Optional<size_t> DecoderHash::last_ref( void ) const
{
  return hashes_.at( 2 );
}

Optional<size_t> DecoderHash::golden_ref( void ) const
{
  return hashes_.at( 3 );
}

Optional<size_t> DecoderHash::alt_ref( void ) const
{
  return hashes_.at( 4 );
}


string DecoderHash::str( void ) const
{
  stringstream hash_str;
  hash_str << hex << uppercase;

  for ( auto hash_iter = hashes_.begin(); hash_iter != hashes_.end(); hash_iter++ ) {
    if ( hash_iter->initialized() ) {
      hash_str << hash_iter->get();
    }
    else {
      hash_str << "x";
    }
    if ( hash_iter != hashes_.end() - 1 ) {
      hash_str << "_";
    }
  }

  return hash_str.str();
}

bool DecoderHash::operator==( const DecoderHash & other ) const
{
  for ( unsigned i = 0; i < hashes_.size(); i++ ) {
    auto & hash = hashes_.at( i );
    auto & other_hash = other.hashes_.at( i );

    if ( hash.initialized() && other_hash.initialized() && hash.get() != other_hash.get() ) {
      return false;
    }
  }
  return true;
}

FullDecoderHash::FullDecoderHash( size_t state_hash, size_t continuation_hash, size_t last_hash, size_t golden_hash, size_t alt_hash )
  : hashes_ { state_hash, continuation_hash, last_hash, golden_hash, alt_hash }
{}

bool FullDecoderHash::can_decode( const DecoderHash & source_hash ) const
{
  if ( source_hash.state().initialized() && source_hash.state().get() != hashes_.at( 0 ) ) {
    return false;
  }

  if ( source_hash.continuation().initialized() && source_hash.continuation().get() != hashes_.at( 1 ) ) {
    return false;
  }

  if ( source_hash.last_ref().initialized() && source_hash.last_ref().get() != hashes_.at( 2 ) ) {
    return false;
  }

  if ( source_hash.golden_ref().initialized() && source_hash.golden_ref().get() != hashes_.at( 3 ) ) {
    return false;
  }

  if ( source_hash.alt_ref().initialized() && source_hash.alt_ref().get() != hashes_.at( 4 ) ) {
    return false;
  }

  return true;
}

void FullDecoderHash::update( const DecoderHash & target_hash )
{
  if ( target_hash.state().initialized() ) {
    hashes_.at( 0 ) = target_hash.state().get();
  }

  if ( target_hash.continuation().initialized() ) {
    hashes_.at( 1 ) = target_hash.continuation().get();
  }

  if ( target_hash.last_ref().initialized() ) {
    hashes_.at( 2 ) = target_hash.last_ref().get();
  }

  if ( target_hash.golden_ref().initialized() ) {
    hashes_.at( 3 ) = target_hash.golden_ref().get();
  }

  if ( target_hash.alt_ref().initialized() ) {
    hashes_.at( 4 ) = target_hash.alt_ref().get();
  }
}

size_t FullDecoderHash::hash( void ) const
{
  return boost::hash_range( hashes_.begin(), hashes_.end() );
}

string FullDecoderHash::str( void ) const
{
  stringstream hash_str;
  hash_str << hex << uppercase;

  hash_str << hashes_.at( 0 ) << "_" << hashes_.at( 1 ) << "_" <<
    hashes_.at( 2 ) << "_" << hashes_.at( 3 ) << "_" << hashes_.at( 4 );

  return hash_str.str();
}
