/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Copyright 2013-2018 the Alfalfa authors
                       and the Massachusetts Institute of Technology

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

      1. Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#ifndef ENCODER_HH
#define ENCODER_HH

#include <vector>
#include <string>
#include <tuple>
#include <limits>

#include "decoder.hh"
#include "frame.hh"
#include "frame_input.hh"
#include "vp8_raster.hh"
#include "ivf_writer.hh"
#include "costs.hh"
#include "enc_state_serializer.hh"
#include "file_descriptor.hh"
#include "block.hh"
#include "frame_pool.hh"

const uint8_t DEFAULT_QUANTIZER = 64;

enum EncoderPass
{
  FIRST_PASS,
  SECOND_PASS
};

enum EncoderQuality
{
  BEST_QUALITY,
  REALTIME_QUALITY
};

enum EncoderMode
{
  MINIMUM_SSIM,
  CONSTANT_QUANTIZER,
  TARGET_FRAME_SIZE,
  REENCODE
};

class SafeReferences
{
public:
  /* For now, we only need the Y planes to do the diamond search, so we only
     keep them in our safe references. */
  SafeRasterHandle last, golden, alternative;

private:
  SafeReferences( const uint16_t width, const uint16_t height );

public:
  SafeReferences( const References & references );

  const SafeRaster & get( reference_frame reference_id ) const;

  static MutableSafeRasterHandle load( const VP8Raster & source );
};

template<class FrameType>
static FramePool<FrameType> & subsampled_frame_pool()
{
  static FramePool<FrameType> pool;
  return pool;
}

class Encoder
{
private:
  struct MBPredictionData
  {
    mbmode prediction_mode { DC_PRED };
    MotionVector mv {};

    uint32_t rate       { std::numeric_limits<uint32_t>::max() };
    uint32_t distortion { std::numeric_limits<uint32_t>::max() };
    uint32_t cost       { std::numeric_limits<uint32_t>::max() };
  };

  struct MVSearchResult
  {
    MotionVector mv;
    size_t first_step;
  };

  static const size_t WIDTH_SAMPLE_DIMENSION_FACTOR { 4 };
  static const size_t HEIGHT_SAMPLE_DIMENSION_FACTOR { 4 };

  typedef SafeArray<SafeArray<std::pair<uint32_t, uint32_t>,
                              MV_PROB_CNT>,
                    2> MVComponentCounts;

  DecoderState decoder_state_;
  uint16_t width() const { return decoder_state_.width; }
  uint16_t height() const { return decoder_state_.height; }
  MutableRasterHandle temp_raster_handle_ { width(), height() };
  References references_;
  SafeReferences safe_references_;

  bool has_state_;

  Costs costs_;

  bool two_pass_encoder_;
  EncoderQuality encode_quality_;

  KeyFrameHandle key_frame_ { width(), height() };
  KeyFrameHandle subsampled_key_frame_ { uint16_t( width() / WIDTH_SAMPLE_DIMENSION_FACTOR ),
      uint16_t( height() / HEIGHT_SAMPLE_DIMENSION_FACTOR ),
      subsampled_frame_pool<KeyFrame>() };
  InterFrameHandle inter_frame_ { width(), height() };
  InterFrameHandle subsampled_inter_frame_ { uint16_t( width() / WIDTH_SAMPLE_DIMENSION_FACTOR ),
      uint16_t( height() / HEIGHT_SAMPLE_DIMENSION_FACTOR ),
      subsampled_frame_pool<InterFrame>() };

  Optional<uint8_t> loop_filter_level_ {};

  /* if set, while encoding with max target size, the search scope for the
     proper quantizer will be:
     last_y_ac_qi_ - a <= y_ac_qi <= last_y_ac_qi_ + a */
  Optional<uint8_t> last_y_ac_qi_ {};

  // TODO: Where did these come from?
  uint32_t RATE_MULTIPLIER { 300 };
  uint32_t DISTORTION_MULTIPLIER { 1 };

  /* this struct will hold stats about the latest encoded frame */
  struct EncodeStats
  {
    Optional<double> ssim;
  } encode_stats_ {};

  static uint32_t rdcost( uint32_t rate, uint32_t distortion,
                          uint32_t rate_multiplier,
                          uint32_t distortion_multiplier );

  template<unsigned int size>
  static uint32_t sad( const VP8Raster::Block<size> & block,
                       const TwoDSubRange<uint8_t, size, size> & prediction );

  template<unsigned int size>
  static uint32_t sse( const VP8Raster::Block<size> & block,
                       const TwoDSubRange<uint8_t, size, size> & prediction );

  template<unsigned int size>
  static uint32_t variance( const VP8Raster::Block<size> & block,
                            const TwoDSubRange<uint8_t, size, size> & prediction );

  MVSearchResult diamond_search( const VP8Raster::Macroblock & original_mb,
                                 VP8Raster::Macroblock & temp_mb,
                                 InterFrameMacroblock & frame_mb,
                                 const VP8Raster & reference,
                                 const SafeRaster & safe_reference,
                                 MotionVector base_mv,
                                 MotionVector origin,
                                 size_t step_size,
                                 const size_t y_ac_qi ) const;

  void luma_mb_inter_predict( const VP8Raster::Macroblock & original_mb,
                              VP8Raster::Macroblock & constructed_mb,
                              VP8Raster::Macroblock & temp_mb,
                              InterFrameMacroblock & frame_mb,
                              const Quantizer & quantizer,
                              MVComponentCounts & component_counts,
                              const size_t y_ac_qi,
                              const EncoderPass encoder_pass );

  void luma_mb_apply_inter_prediction( const VP8Raster::Macroblock & original_mb,
                                       VP8Raster::Macroblock & reconstructed_mb,
                                       InterFrameMacroblock & frame_mb,
                                       const Quantizer & quantizer,
                                       const mbmode best_pred,
                                       const MotionVector best_mv );

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
                                                 const EncoderPass encoder_pass = FIRST_PASS,
                                                 const bool interframe = false ) const;

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
                                                   VP8Raster::Macroblock & temp_mb,
                                                   const bool interframe = false ) const;

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
                                const EncoderPass encoder_pass = FIRST_PASS,
                                const bool interframe = false ) const;

  bmode luma_sb_intra_predict( const VP8Raster::Block4 & original_sb,
                               VP8Raster::Block4 & constructed_sb,
                               VP8Raster::Block4 & temp_sb,
                               const SafeArray<uint16_t, num_intra_b_modes> & mode_costs ) const;

  void luma_sb_apply_intra_prediction( const VP8Raster::Block4 & original_sb,
                                       VP8Raster::Block4 & reconstructed_sb,
                                       YBlock & frame_sb,
                                       const Quantizer & quantizer,
                                       bmode sb_prediction_mode,
                                       const EncoderPass encoder_pass ) const;

  template<class FrameSubblockType>
  void trellis_quantize( FrameSubblockType & frame_sb,
                         const Quantizer & quantizer ) const;

  void check_reset_y2( Y2Block & y2, const Quantizer & quantizer ) const;

  VP8Raster & temp_raster() { return temp_raster_handle_.get(); }

  /* this function returns the ssim value as the output */
  template<class FrameType>
  void apply_best_loopfilter_settings( const VP8Raster & original,
                                       VP8Raster & reconstructed,
                                       FrameType & frame );

  template<class FrameType>
  void optimize_probability_tables( FrameType & frame, const TokenBranchCounts & token_branch_counts );

  template<class FrameHeaderType, class MacroblockType>
  void optimize_prob_skip( Frame<FrameHeaderType, MacroblockType> & frame );

  void optimize_interframe_probs( InterFrame & frame );
  void optimize_mv_probs( InterFrame & frame, const MVComponentCounts & counts );

  void update_mv_component_counts( const int16_t & component,
                                   const bool is_x,
                                   MVComponentCounts & counts ) const;

  static unsigned calc_prob( unsigned false_count, unsigned total );

  template<class FrameType>
  std::vector<uint8_t> write_frame( const FrameType & frame );

  template<class FrameType>
  std::vector<uint8_t> write_frame( const FrameType & frame, const ProbabilityTables & prob_tables );


  /* Encoded frame size estimation */
  template<class FrameType>
  size_t estimate_size( const VP8Raster & raster, const size_t y_ac_qi );

  /* Convergence-related stuff */
  template<class FrameType>
  InterFrame reencode_as_interframe( const VP8Raster & unfiltered_output,
                                       const FrameType & original_frame,
                                       const QuantIndices & quant_indices );

  InterFrame update_residues( const VP8Raster & unfiltered_output,
                                const InterFrame & original_frame,
                                const QuantIndices & quant_indices,
                                const bool last_frame );

  void update_macroblock( const VP8Raster::Macroblock & original_rmb,
                          VP8Raster::Macroblock & reconstructed_rmb,
                          VP8Raster::Macroblock & temp_mb,
                          InterFrameMacroblock & frame_mb,
                          const InterFrameMacroblock & original_fmb,
                          const Quantizer & quantizer );

  template<class FrameType>
  void update_decoder_state( const FrameType & frame );

  template<class FrameType>
  std::pair<FrameType &, double> encode_raster( const VP8Raster & raster,
                                                const QuantIndices & quant_indices,
                                                const bool update_state = false,
                                                const bool compute_ssim = false );

  template<class FrameType>
  FrameType & encode_with_quantizer_search( const VP8Raster & raster,
                                            const double minimum_ssim );

  void update_rd_multipliers( const Quantizer & quantizer );

public:
  Encoder( const uint16_t s_width, const uint16_t s_height,
           const bool two_pass,
           const EncoderQuality quality );

  Encoder( const Decoder & decoder, const bool two_pass,
           const EncoderQuality quality );

  Encoder( const Encoder & encoder );

  Encoder( Encoder && encoder );

  Encoder & operator=( Encoder && encoder );

  std::vector<uint8_t> encode_with_minimum_ssim( const VP8Raster & raster,
                                                 const double minimum_ssim );

  std::vector<uint8_t> encode_with_quantizer( const VP8Raster & raster,
                                              const uint8_t y_ac_qi );

  /* Tries to encode the given raster with the best possible quality, without
   * exceeding the target size. */
  std::vector<uint8_t> encode_with_target_size( const VP8Raster & raster,
                                                const size_t target_size );

  void reencode( const std::vector<RasterHandle> & original_rasters,
                 const std::vector<std::pair<Optional<KeyFrame>, Optional<InterFrame> > > & prediction_frames,
                 const double kf_q_weight,
                 const bool extra_frame_chunk,
                 IVFWriter & ivf_writer );

  size_t estimate_frame_size( const VP8Raster & raster, const size_t y_ac_qi );

  Decoder export_decoder() const { return { decoder_state_, references_ }; }

  EncodeStats stats() { return encode_stats_; }

  uint32_t minihash() const;
};

#endif /* ENCODER_HH */
