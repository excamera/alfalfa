#ifndef SERIALIZED_FRAME_HH
#define SERIALIZED_FRAME_HH

#include <string>
#include <vector>

#include "decoder.hh"

class SerializedFrame
{
private:
  std::vector<uint8_t> frame_;
  DecoderHash source_hash_, target_hash_;

  // FIXME turn output_raster_ into output_hash_, calc PSNR at construction time
  Optional<RasterHandle> output_raster_;

public:
  SerializedFrame( const std::string & path );

  SerializedFrame( const std::vector<uint8_t> & frame,
                   const DecoderHash & source_hash, const DecoderHash & target_hash,
                   const Optional<RasterHandle> & output );

  SerializedFrame( const Chunk & frame,
		   const DecoderHash & source_hash, const DecoderHash & target_hash,
                   const Optional<RasterHandle> & output );

  Chunk chunk( void ) const;

  bool validate_source( const DecoderHash & decoder ) const;

  bool validate_target( const DecoderHash & decoder ) const;

  std::string name( void ) const;

  bool shown( void ) const { return output_raster_.initialized(); }

  RasterHandle get_output( void ) const { return output_raster_.get(); }

  double psnr( const RasterHandle & original ) const;

  unsigned size( void ) const;

  void write( const std::string & path = "" ) const;
};

#endif
