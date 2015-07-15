#ifndef DECODER_HASH_INCLUDED
#define DECODER_HASH_INCLUDED

#include "optional.hh"
#include "safe_array.hh"
#include "dependency_tracker.hh"

#include <string>

//FIXME add DecoderHash, HashDecoder contains DecoderHash, DecoderHash is used to implement SerializedFrame hash names

class DecoderHash {
private:
  SafeArray<Optional<size_t>, 5> hashes_;

public:
  DecoderHash( const std::string & frame_name, bool target );
  DecoderHash( Optional<size_t> state_hash, Optional<size_t> continuation_hash,
               Optional<size_t> last_hash, Optional<size_t> golden_hash,
               Optional<size_t> alt_hash );
  DecoderHash( size_t state_hash, size_t continuation_hash, size_t last_hash,
               size_t golden_hash, size_t alt_hash );

  DecoderHash filter( const DependencyTracker & tracker ) const;

  size_t last_ref( void ) const;
  size_t golden_ref( void ) const;
  size_t alt_ref( void ) const;

  std::string str() const;

  bool operator==( const DecoderHash & other ) const;
};

#endif
