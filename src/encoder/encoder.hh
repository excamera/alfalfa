#ifndef ENCODER_HH
#define ENCODER_HH

#include <vector>
#include <string>

#include "vp8_raster.hh"
#include "frame.hh"
#include "ivf_writer.hh"

enum Component {
  Y_COMPONENT, U_COMPONENT, V_COMPONENT
};

class Encoder
{
private:
  IVFWriter ivf_writer_;

  template <class MacroblockType>
  std::pair< mbmode, TwoD< uint8_t > > luma_mb_intra_predict( VP8Raster::Macroblock & original_mb,
                                                              VP8Raster::Macroblock & constructed_mb,
                                                              MacroblockType & frame_mb );

  template <class MacroblockType>
  std::pair< mbmode, TwoD< uint8_t > > chroma_mb_intra_predict( VP8Raster::Macroblock & original_mb,
                                                                VP8Raster::Macroblock & constructed_mb,
                                                                MacroblockType & frame_mb,
                                                                Component component );

  template <class MacroblockType>
  std::pair< bmode, TwoD< uint8_t > > luma_sb_intra_predict( VP8Raster::Macroblock & original_mb,
                                                             VP8Raster::Macroblock & constructed_mb,
                                                             MacroblockType & frame_mb,
                                                             VP8Raster::Block4 & subblock );

public:
  Encoder( const std::string & output_filename, uint16_t width, uint16_t height );
  double encode_as_keyframe( const VP8Raster & raster, double minimum_ssim );

  std::pair< KeyFrame, double > get_encoded_keyframe( const VP8Raster & raster, const Optional<QuantIndices> quant_indices = Optional<QuantIndices>() );

  static KeyFrame make_empty_frame( unsigned int width, unsigned int height );
};

#endif /* ENCODER_HH */
