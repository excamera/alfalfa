#include "decoder_hash.hh"

#include <sstream>
#include <string>

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

DecoderHash::DecoderHash( const std::string & frame_name, bool target )
  : hashes_( target ? split_target( frame_name ) : split_source( frame_name ) )
{}

DecoderHash::DecoderHash( Optional<size_t> state_hash, Optional<size_t> continuation_hash,
                          Optional<size_t> last_hash, Optional<size_t> golden_hash,
                          Optional<size_t> alt_hash )
  : hashes_ { state_hash, continuation_hash, last_hash, golden_hash, alt_hash }
{}

DecoderHash::DecoderHash( size_t state_hash, size_t continuation_hash, size_t last_hash,
                          size_t golden_hash, size_t alt_hash )
  : DecoderHash( Optional<size_t>( state_hash ), Optional<size_t>( continuation_hash ),
                 Optional<size_t>( last_hash ), Optional<size_t>( golden_hash ), Optional<size_t>( alt_hash ) )
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

size_t DecoderHash::last_ref( void ) const
{
  return hashes_.at( 2 ).get();
}

size_t DecoderHash::golden_ref( void ) const
{
  return hashes_.at( 3 ).get();
}

size_t DecoderHash::alt_ref( void ) const
{
  return hashes_.at( 4 ).get();
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
