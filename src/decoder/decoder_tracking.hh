#ifndef DECODER_HASH_INCLUDED
#define DECODER_HASH_INCLUDED

#include "optional.hh"
#include "safe_array.hh"
#include "modemv_data.hh"

#include <string>
#include <initializer_list>

// FIXME, interframe doesn't necessarily update state or depend on state
// but it just works if we assume interframe always depends on and updates
// state.

struct DependencyTracker
{
  bool need_state, need_continuation, need_last, need_golden, need_alternate;

  bool & reference( const reference_frame reference_id );
};

class DecoderHash;

struct SourceHash
{
  Optional<size_t> state_hash, continuation_hash,
                   last_hash, golden_hash, alt_hash;

  SourceHash( const std::string & frame_name );
  SourceHash( const Optional<size_t> & state, const Optional<size_t> & continuation,
              const Optional<size_t> & last, const Optional<size_t> & golden,
              const Optional<size_t> & alt );

  using CheckFunc = bool ( * )( const SourceHash & source, const DecoderHash & decoder );

  CheckFunc check;

  std::string str() const;
};

struct UpdateTracker
{
  bool update_last;
  bool update_golden;
  bool update_alternate;
  bool last_to_golden;
  bool last_to_alternate;
  bool golden_to_alternate;
  bool alternate_to_golden;

  UpdateTracker( bool set_update_last, bool set_update_golden,
                 bool set_update_alternate, bool set_last_to_golden,
                 bool set_last_to_alternate, bool set_golden_to_alternate,
                 bool set_alternate_to_golden );
};

struct MissingTracker
{
  bool last;
  bool golden;
  bool alternate;
};

struct TargetHash : public UpdateTracker
{
  size_t state_hash, continuation_hash, output_hash;
  bool shown;

  TargetHash( const std::string & frame_name );
  TargetHash( const UpdateTracker & updates, size_t state,
              size_t continuation, size_t output, bool shown );

  std::string str() const;
};

class DecoderHash
{
private:
  size_t state_hash_, continuation_hash_, last_hash_, golden_hash_, alt_hash_;
public:
  DecoderHash( size_t state_hash, size_t continuation_hash, size_t last_hash,
               size_t golden_hash, size_t alt_hash );

  bool can_decode( const SourceHash & source_hash ) const;

  void update( const TargetHash & target_hash );

  size_t state_hash( void ) const;

  size_t continuation_hash( void ) const;

  size_t last_hash( void ) const;

  size_t golden_hash( void ) const;

  size_t alt_hash( void ) const;

  size_t hash( void ) const;

  std::string str( void ) const;

  bool operator==( const DecoderHash & other ) const;
};

#endif
