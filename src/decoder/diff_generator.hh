#ifndef DIFF_GENERATOR_HH
#define DIFF_GENERATOR_HH

#include "decoder.hh"
#include "frame.hh"

/* Specialized version of Decoder which holds all necessary
 * state for generating diffs between streams */
class DiffGenerator : public Decoder
{
private:
  bool on_key_frame_;
  Optional<KeyFrame> key_frame_ {};
  Optional<InterFrame> inter_frame_ {};

  RasterHandle raster_ { state_.width, state_.height };

  template< class FrameType >
  void update_diff_state( const FrameType & frame, const RasterHandle & raster );

public:
  DiffGenerator( const uint16_t width, const uint16_t height );

  bool decode_frame( const Chunk & frame, RasterHandle & raster );

  RasterHandle decode_diff( const Chunk & frame ) const;

  std::vector< uint8_t > operator-( const DiffGenerator & source_decoder );

};
#endif
