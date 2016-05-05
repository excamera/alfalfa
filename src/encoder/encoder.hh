#ifndef ENCODER_HH
#define ENCODER_HH

#include <vector>
#include <string>
#include <tuple>

#include "vp8_raster.hh"
#include "frame.hh"
#include "ivf_writer.hh"

class Encoder
{
private:
  IVFWriter ivf_writer_;

  template<unsigned int size>
  static uint32_t get_variance( const VP8Raster::Block< size > & block,
                              const TwoD< uint8_t > & prediction,
                              const uint16_t dc_factor, const uint16_t ac_factor );

  template<unsigned int size>
  static uint32_t get_variance( const VP8Raster::Block< size > & block,
                              const TwoDSubRange< uint8_t, size, size > & prediction,
                              const uint16_t dc_factor, const uint16_t ac_factor );

  template <class MacroblockType>
  void luma_mb_intra_predict( VP8Raster::Macroblock & original_mb,
                              VP8Raster::Macroblock & constructed_mb,
                              MacroblockType & frame_mb,
                              Quantizer & quantizer );

  template <class MacroblockType>
  void chroma_mb_intra_predict( VP8Raster::Macroblock & original_mb,
                                VP8Raster::Macroblock & constructed_mb,
                                MacroblockType & frame_mb,
                                Quantizer & quantizer );

  std::pair< bmode, TwoD< uint8_t > > luma_sb_intra_predict( VP8Raster::Block4 & original_sb,
                                                             VP8Raster::Block4 & constructed_sb,
                                                             YBlock & frame_sb,
                                                             Quantizer & quantizer );

public:
  Encoder( const std::string & output_filename, uint16_t width, uint16_t height );
  double encode_as_keyframe( const VP8Raster & raster, double minimum_ssim );

  std::pair< KeyFrame, double > get_encoded_keyframe( const VP8Raster & raster, const Optional<QuantIndices> quant_indices = Optional<QuantIndices>() );

  static KeyFrame make_empty_frame( unsigned int width, unsigned int height );
};

#endif /* ENCODER_HH */
