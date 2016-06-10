#ifndef ENCODER_HH
#define ENCODER_HH

#include <vector>
#include <string>
#include <tuple>
#include <limits>

#include "frame.hh"
#include "vp8_raster.hh"
#include "ivf_writer.hh"
#include "costs.hh"

enum EncoderPass
{
  FIRST_PASS,
  SECOND_PASS
};

class Encoder
{
private:
  IVFWriter ivf_writer_;
  uint16_t width_;
  uint16_t height_;
  DecoderState decoder_state_;
  Costs costs_;

  double minimum_ssim_ { 0.8 };
  bool two_pass_encoder_ { false };

  // TODO: Where did these come from? Are these the possible values?
  uint32_t RATE_MULTIPLIER { 300 };
  uint32_t DISTORTION_MULTIPLIER { 1 };

  static uint8_t token_for_coeff( int16_t coeff );

  static uint32_t rdcost( uint32_t rate, uint32_t distortion,
                          uint32_t rate_multiplier,
                          uint32_t distortion_multiplier );

  template<unsigned int size>
  static uint32_t sse( const VP8Raster::Block<size> & block,
                       const TwoD<uint8_t> & prediction );

  template<unsigned int size>
  static uint32_t sse( const VP8Raster::Block<size> & block,
                       const TwoDSubRange<uint8_t, size, size> & prediction );

  template<unsigned int size>
  static uint32_t variance( const VP8Raster::Block<size> & block,
                            const TwoD<uint8_t> & prediction );

  template<unsigned int size>
  static uint32_t variance( const VP8Raster::Block<size> & block,
                            const TwoDSubRange<uint8_t, size, size> & prediction );

  template <class MacroblockType>
  void luma_mb_intra_predict( const VP8Raster::Macroblock & original_mb,
                              VP8Raster::Macroblock & constructed_mb,
                              MacroblockType & frame_mb,
                              const Quantizer & quantizer,
                              const EncoderPass encoder_pass = FIRST_PASS ) const;

  template <class MacroblockType>
  void chroma_mb_intra_predict( const VP8Raster::Macroblock & original_mb,
                                VP8Raster::Macroblock & constructed_mb,
                                MacroblockType & frame_mb,
                                const Quantizer & quantizer,
                                const EncoderPass encoder_pass = FIRST_PASS ) const;

  std::pair<bmode, TwoD<uint8_t>> luma_sb_intra_predict( const VP8Raster::Block4 & original_sb,
                                                         VP8Raster::Block4 & constructed_sb,
                                                         const SafeArray<uint16_t, num_intra_b_modes> & mode_costs ) const;

  template<class FrameType>
  std::pair<KeyFrame, double> encode_with_quantizer( const VP8Raster & raster, const QuantIndices & quant_indices );

  template<class FrameType>
  void optimize_probability_tables( FrameType & frame, const TokenBranchCounts & token_branch_counts );

  template<class FrameSubblockType>
  void trellis_quantize( FrameSubblockType & frame_sb,
                         const Quantizer & quantizer ) const;

  void check_reset_y2( Y2Block & y2, const Quantizer & quantizer ) const;

public:
  Encoder( const std::string & output_filename, const uint16_t width,
           const uint16_t height, const bool two_pass );

  double encode_as_keyframe( const VP8Raster & raster,
                             const double minimum_ssim,
                             const uint8_t y_ac_qi = std::numeric_limits<uint8_t>::max() );

  static KeyFrame make_empty_frame( const uint16_t width, const uint16_t height );
};

#endif /* ENCODER_HH */
