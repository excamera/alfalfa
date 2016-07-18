/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

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

struct ReferenceFlags
{
  bool has_last, has_golden, has_alternative;

  ReferenceFlags()
    : has_last( false ), has_golden( false ), has_alternative( false )
  {}

  void clear_all()
  {
    has_last = false;
    has_golden = false;
    has_alternative = false;
  }
};

class Encoder
{
private:
  struct MBPredictionData
  {
    mbmode prediction_mode { DC_PRED };

    uint32_t rate       { std::numeric_limits<uint32_t>::max() };
    uint32_t distortion { std::numeric_limits<uint32_t>::max() };
    uint32_t cost       { std::numeric_limits<uint32_t>::max() };
  };

  IVFWriter ivf_writer_;
  uint16_t width_;
  uint16_t height_;
  MutableRasterHandle temp_raster_handle_;
  DecoderState decoder_state_;
  References references_;
  ReferenceFlags reference_flags_;

  Costs costs_;

  double minimum_ssim_ { 0.8 };
  bool two_pass_encoder_ { false };

  // TODO: Where did these come from?
  uint32_t RATE_MULTIPLIER { 300 };
  uint32_t DISTORTION_MULTIPLIER { 1 };

  static uint32_t rdcost( uint32_t rate, uint32_t distortion,
                          uint32_t rate_multiplier,
                          uint32_t distortion_multiplier );

  template<unsigned int size>
  static uint32_t sse( const VP8Raster::Block<size> & block,
                       const TwoDSubRange<uint8_t, size, size> & prediction );

  template<unsigned int size>
  static uint32_t variance( const VP8Raster::Block<size> & block,
                            const TwoDSubRange<uint8_t, size, size> & prediction );

  void luma_mb_inter_predict( const VP8Raster::Macroblock & original_mb,
                              VP8Raster::Macroblock & constructed_mb,
                              VP8Raster::Macroblock & temp_mb,
                              InterFrameMacroblock & frame_mb,
                              const Quantizer & quantizer,
                              const EncoderPass encoder_pass = FIRST_PASS ) const;

  void chroma_mb_inter_predict( const VP8Raster::Macroblock & original_mb,
                                VP8Raster::Macroblock & constructed_mb,
                                VP8Raster::Macroblock & temp_mb,
                                InterFrameMacroblock & frame_mb,
                                const Quantizer & quantizer,
                                const EncoderPass encoder_pass = FIRST_PASS ) const;

  template<class MacroblockType>
  MBPredictionData luma_mb_best_prediction_mode( const VP8Raster::Macroblock & original_mb,
                                                 VP8Raster::Macroblock & reconstructed_mb,
                                                 VP8Raster::Macroblock & temp_mb,
                                                 MacroblockType & frame_mb,
                                                 const Quantizer & quantizer,
                                                 const EncoderPass encoder_pass = FIRST_PASS ) const;

  template<class MacroblockType>
  void luma_mb_apply_intra_prediction( const VP8Raster::Macroblock & original_mb,
                                       VP8Raster::Macroblock & reconstructed_mb,
                                       VP8Raster::Macroblock & temp_mb,
                                       MacroblockType & frame_mb,
                                       const Quantizer & quantizer,
                                       const mbmode min_prediction_mode,
                                       const EncoderPass encoder_pass = FIRST_PASS ) const;

  template<class MacroblockType>
  void luma_mb_intra_predict( const VP8Raster::Macroblock & original_mb,
                              VP8Raster::Macroblock & constructed_mb,
                              VP8Raster::Macroblock & temp_mb,
                              MacroblockType & frame_mb,
                              const Quantizer & quantizer,
                              const EncoderPass encoder_pass = FIRST_PASS ) const;

  MBPredictionData chroma_mb_best_prediction_mode( const VP8Raster::Macroblock & original_mb,
                                                   VP8Raster::Macroblock & reconstructed_mb,
                                                   VP8Raster::Macroblock & temp_mb ) const;

  template<class MacroblockType>
  void chroma_mb_apply_intra_prediction( const VP8Raster::Macroblock & original_mb,
                                         VP8Raster::Macroblock & reconstructed_mb,
                                         __attribute__((unused)) VP8Raster::Macroblock & temp_mb,
                                         MacroblockType & frame_mb,
                                         const Quantizer & quantizer,
                                         const mbmode min_prediction_mode,
                                         const EncoderPass encoder_pass ) const;


  template<class MacroblockType>
  void chroma_mb_intra_predict( const VP8Raster::Macroblock & original_mb,
                                VP8Raster::Macroblock & constructed_mb,
                                VP8Raster::Macroblock & temp_mb,
                                MacroblockType & frame_mb,
                                const Quantizer & quantizer,
                                const EncoderPass encoder_pass = FIRST_PASS ) const;

  bmode luma_sb_intra_predict( const VP8Raster::Block4 & original_sb,
                               VP8Raster::Block4 & constructed_sb,
                               VP8Raster::Block4 & temp_sb,
                               const SafeArray<uint16_t, num_intra_b_modes> & mode_costs ) const;

  template<class FrameType>
  std::pair<FrameType, double> encode_with_quantizer( const VP8Raster & raster, const QuantIndices & quant_indices );

  template<class FrameType>
  double encode_raster( const VP8Raster & raster, const double minimum_ssim,
                        const uint8_t y_ac_qi = std::numeric_limits<uint8_t>::max() );

  template<class FrameType>
  void optimize_probability_tables( FrameType & frame, const TokenBranchCounts & token_branch_counts );

  template<class FrameSubblockType>
  void trellis_quantize( FrameSubblockType & frame_sb,
                         const Quantizer & quantizer ) const;

  void check_reset_y2( Y2Block & y2, const Quantizer & quantizer ) const;

  VP8Raster & temp_raster() { return temp_raster_handle_.get(); }

  bool should_encode_as_keyframe( const VP8Raster & raster );

  template<class FrameType>
  void apply_best_loopfilter_settings( const VP8Raster & original,
                                       VP8Raster & reconstructed,
                                       FrameType & frame );

  template<class FrameHeaderType, class MacroblockType>
  void optimize_prob_skip( Frame<FrameHeaderType, MacroblockType> & frame );

  void optimize_interframe_probs( InterFrame & frame );

public:
  Encoder( const std::string & output_filename, const uint16_t width,
           const uint16_t height, const bool two_pass );

  /*
   * This function decides that the current raster should be encoded as a
   * keyframe or as an interframe.
   */
  double encode( const VP8Raster & raster,
                 const double minimum_ssim,
                 const uint8_t y_ac_qi = std::numeric_limits<uint8_t>::max() );

  template<class FrameType>
  static FrameType make_empty_frame( const uint16_t width, const uint16_t height );

  static unsigned calc_prob( unsigned false_count, unsigned total );
};

#endif /* ENCODER_HH */
