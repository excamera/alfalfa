#ifndef DEPENDENCY_TRACKING_HH
#define DEPENDENCY_TRACKING_HH

#include "optional.hh"
#include "safe_array.hh"
#include "modemv_data.hh"
#include "raster_handle.hh"
#include "protobufs/alfalfa.pb.h"

#include <string>
#include <initializer_list>
#include <boost/functional/hash.hpp>

// FIXME, interframe doesn't necessarily update state or depend on state
// but it just works if we assume interframe always depends on and updates
// state.

struct DependencyTracker
{
  bool need_state, need_last, need_golden, need_alternate;

  bool & reference( const reference_frame reference_id );
};

class DecoderHash;

struct SourceHash
{
  Optional<size_t> state_hash, last_hash, golden_hash, alt_hash;

  SourceHash( const std::string & frame_name );
  SourceHash( const Optional<size_t> & state, const Optional<size_t> & last,
              const Optional<size_t> & golden, const Optional<size_t> & alt );
  SourceHash( const AlfalfaProtobufs::SourceHash & message );

  AlfalfaProtobufs::SourceHash to_protobuf() const;

  std::string str() const;

  bool operator==( const SourceHash & other ) const;
  bool operator!=( const SourceHash & other ) const;
};

struct UpdateTracker
{
public:
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
  size_t state_hash, output_hash;
  bool shown;

  TargetHash( const std::string & frame_name );
  TargetHash( const UpdateTracker & updates, size_t state, size_t output, bool shown );
  TargetHash( const AlfalfaProtobufs::TargetHash & message );

  AlfalfaProtobufs::TargetHash to_protobuf() const;

  std::string str() const;

  bool operator==( const TargetHash & other ) const;
  bool operator!=( const TargetHash & other ) const;
};

class DecoderHash
{
private:
  size_t state_hash_, last_hash_, golden_hash_, alt_hash_;
public:
  DecoderHash( size_t state_hash, size_t last_hash,
               size_t golden_hash, size_t alt_hash );

  bool can_decode( const SourceHash & source_hash ) const;

  void update( const TargetHash & target_hash );

  size_t state_hash( void ) const;

  size_t last_hash( void ) const;

  size_t golden_hash( void ) const;

  size_t alt_hash( void ) const;

  size_t hash( void ) const;

  std::string str( void ) const;

  bool operator==( const DecoderHash & other ) const;

  bool operator!=( const DecoderHash & other ) const;
};

namespace std
{
  template<>
  struct hash<SourceHash>
  {
    size_t operator()( const SourceHash & hash ) const
    {
      size_t hash_val = 0;

      boost::hash_combine( hash_val, hash.state_hash.initialized() ? hash.state_hash.get() : 0 );
      boost::hash_combine( hash_val, hash.last_hash.initialized() ? hash.last_hash.get() : 0 );
      boost::hash_combine( hash_val, hash.golden_hash.initialized() ? hash.golden_hash.get() : 0 );
      boost::hash_combine( hash_val, hash.alt_hash.initialized() ? hash.alt_hash.get() : 0 );

      return hash_val;
    }
  };

  template<>
  struct hash<TargetHash>
  {
    size_t operator()( const TargetHash & hash ) const
    {
      size_t hash_val = 0;

      boost::hash_combine( hash_val, hash.state_hash );
      boost::hash_combine( hash_val, hash.output_hash );
      boost::hash_combine( hash_val, hash.update_last );
      boost::hash_combine( hash_val, hash.update_golden );
      boost::hash_combine( hash_val, hash.update_alternate );
      boost::hash_combine( hash_val, hash.last_to_golden );
      boost::hash_combine( hash_val, hash.last_to_alternate );
      boost::hash_combine( hash_val, hash.golden_to_alternate );
      boost::hash_combine( hash_val, hash.alternate_to_golden );
      boost::hash_combine( hash_val, hash.shown );

      return hash_val;
    }
  };

  template<>
  struct equal_to<SourceHash>
  {
    bool operator()( const SourceHash & lhs, const SourceHash & rhs ) const
    {
      return lhs == rhs;
    }
  };

  template<>
  struct equal_to<TargetHash>
  {
    bool operator()( const TargetHash & lhs, const TargetHash & rhs ) const
    {
      return lhs == rhs;
    }
  };

}

#endif
