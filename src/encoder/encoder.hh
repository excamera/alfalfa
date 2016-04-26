#ifndef ENCODER_HH
#define ENCODER_HH

#include <vector>
#include <string>

#include "vp8_raster.hh"
#include "frame.hh"
#include "ivf_writer.hh"

class Encoder {
private:
  IVFWriter ivf_writer_;

public:
  Encoder( const std::string & output_filename, uint16_t width, uint16_t height );

  void encode_as_keyframe( const VP8Raster & raster );

  static KeyFrame make_empty_frame( unsigned int width, unsigned int height );  
};

#endif /* ENCODER_HH */
