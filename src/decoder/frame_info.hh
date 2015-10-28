#ifndef FRAME_INFO_HH
#define FRAME_INFO_HH

#include <string>
#include <fstream>

#include "decoder.hh"
#include "dependency_tracking.hh"
#include "serialized_frame.hh"

using namespace std;

std::string build_frame_name( const SourceHash & source_hash,
  const TargetHash & target_hash );

class FrameInfo
{
private:
  Optional<std::string> ivf_filename_;
  size_t offset_;
  size_t length_;
  SourceHash source_hash_;
  TargetHash target_hash_;

public:
  std::string frame_name() const { return str(); }
  Optional<std::string> ivf_filename() const { return ivf_filename_; }
  size_t offset() const { return offset_; }
  size_t length() const { return length_; }
  const SourceHash & source_hash() const { return source_hash_; }
  const TargetHash & target_hash() const { return target_hash_; }

  void set_ivf_filename( const std::string & filename ) { ivf_filename_ = make_optional( true, filename ); }
  void set_source_hash( const SourceHash & source_hash ) { source_hash_ = source_hash; }
  void set_target_hash( const TargetHash & target_hash ) { target_hash_ = target_hash; }
  void set_offset( const size_t & offset ) { offset_ = offset; }
  void set_length( const size_t & length ) { length_ = length; }

  FrameInfo();

  FrameInfo( const std::string & frame_name, const size_t & offset,
    const size_t & length );

  FrameInfo( const size_t & offset, const size_t & length,
    const SourceHash & source_hash,
    const TargetHash & target_hash );

  bool validate_source( const DecoderHash & decoder_hash ) const;
  bool validate_target( const DecoderHash & decoder_hash ) const;
  bool shown( void ) const;

  std::string str() const { return build_frame_name( source_hash_, target_hash_ ); }
};

#endif /* FRAME_INFO_HH */
