#ifndef SERIALIZED_FRAME_HH
#define SERIALIZED_FRAME_HH

#include <string>
#include <vector>

#include "decoder.hh"

class SerializedFrame
{
private:
  std::vector<uint8_t> frame_;
  std::string source_hash_, target_hash_;

public:
  SerializedFrame( const std::string & path );

  SerializedFrame( const std::vector<uint8_t> & frame, const std::string & source_hash,
		   const std::string & target_hash );

  SerializedFrame( const Chunk & frame, const std::string & source_hash,
		   const std::string & target_hash );

  Chunk chunk( void ) const;

  bool validate_source( const Decoder & decoder ) const;

  bool validate_target( const Decoder & decoder ) const;

  std::string name( void ) const;

  unsigned size( void ) const;

  void write( const std::string & path = "" ) const;
};

#endif
