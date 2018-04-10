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

#ifndef DECODER_HH
#define DECODER_HH

#include <vector>
#include "safe_array.hh"
#include "modemv_data.hh"
#include "loopfilter.hh"
#include "vp8_prob_data.hh"
#include "quantization.hh"
#include "exception.hh"
#include "raster_handle.hh"
#include "uncompressed_chunk.hh"
#include "frame_header.hh"
#include "enc_state_serializer.hh"

class Chunk;
class VP8Raster;
struct KeyFrameHeader;
struct InterFrameHeader;

template<unsigned int size>
static void assign( SafeArray< Probability, size > & dest, const Array< Unsigned<8>, size > & src )
{
  for ( unsigned int i = 0; i < size; i++ ) {
    dest.at( i ) = src.at( i );
  }
}

struct ProbabilityTables
{
  SafeArray<SafeArray<SafeArray<SafeArray<Probability,
                                          ENTROPY_NODES>,
                                PREV_COEF_CONTEXTS>,
                      COEF_BANDS>,
            BLOCK_TYPES> coeff_probs = k_default_coeff_probs;

  ProbabilityArray<num_y_modes> y_mode_probs = k_default_y_mode_probs;
  ProbabilityArray<num_uv_modes> uv_mode_probs = k_default_uv_mode_probs;

  SafeArray<SafeArray<Probability, MV_PROB_CNT>, 2> motion_vector_probs = k_default_mv_probs;

  ProbabilityTables() {}

  ProbabilityTables(EncoderStateDeserializer &idata);

  template <class HeaderType>
  void coeff_prob_update( const HeaderType & header );
  void mv_prob_update( const Enumerate<Enumerate<MVProbUpdate, MV_PROB_CNT>, 2> & mv_prob_updates );

  template <class HeaderType>
  void update( const HeaderType & header );

  size_t hash( void ) const;

  bool operator==( const ProbabilityTables & other ) const;

  bool operator!=( const ProbabilityTables & other ) const { return not operator==( other ); }

  uint32_t serialize(EncoderStateSerializer &odata) const;
  static ProbabilityTables deserialize(EncoderStateDeserializer &idata) {
    return ProbabilityTables(idata);
  }
};

struct FilterAdjustments
{
  /* Adjustments to the deblocking filter based on the macroblock's reference frame */
  SafeArray< int8_t, num_reference_frames > loopfilter_ref_adjustments {{}};

  /* Adjustments based on the macroblock's prediction mode */
  SafeArray< int8_t, 4 > loopfilter_mode_adjustments {{}};

  FilterAdjustments() {}

  FilterAdjustments(EncoderStateDeserializer &idata);

  template <class HeaderType>
  FilterAdjustments( const HeaderType & header ) { update( header ); }

  template <class HeaderType>
  void update( const HeaderType & header );

  size_t hash( void ) const;

  bool operator==( const FilterAdjustments & other ) const;

  bool operator!=( const FilterAdjustments & other ) const { return not operator==( other ); }

  size_t serialize(EncoderStateSerializer &odata) const;
  static FilterAdjustments deserialize(EncoderStateDeserializer &idata) {
    return FilterAdjustments(idata);
  }
};

struct References
{
  RasterHandle last, golden, alternative;

  References( const uint16_t width, const uint16_t height );

  References( MutableRasterHandle && raster );

  References(EncoderStateDeserializer &idata, const uint16_t width, const uint16_t height);

  const VP8Raster & at( const reference_frame reference_id ) const
  {
    switch ( reference_id ) {
    case LAST_FRAME: return last;
    case GOLDEN_FRAME: return golden;
    case ALTREF_FRAME: return alternative;
    default: throw LogicError();
    }
  }

  bool operator==( const References & other ) const;

  bool operator!=( const References & other ) const { return not operator==( other ); }

  size_t serialize(EncoderStateSerializer &odata) const;
  static References deserialize(EncoderStateDeserializer &idata);
};

using SegmentationMap = TwoD< uint8_t >;

struct Segmentation
{
  /* Whether segment-based adjustments are absolute or relative */
  bool absolute_segment_adjustments {};

  /* Segment-based adjustments to the quantizer */
  SafeArray< int8_t, num_segments > segment_quantizer_adjustments {{}};

  /* Segment-based adjustments to the in-loop deblocking filter */
  SafeArray< int8_t, num_segments > segment_filter_adjustments {{}};

  /* Mapping of macroblocks to segments */
  SegmentationMap map;

  template <class HeaderType>
  Segmentation( const HeaderType & header,
                const unsigned int width,
                const unsigned int height );

  Segmentation(EncoderStateDeserializer &idata, const bool abs, const unsigned width, const unsigned height);

  // testing only
  Segmentation(const unsigned width, const unsigned height);

  template <class HeaderType>
  void update( const HeaderType & header );

  size_t hash( void ) const;

  bool operator==( const Segmentation & other ) const;

  Segmentation( const Segmentation & other );

  size_t serialize(EncoderStateSerializer &odata) const;
  static Segmentation deserialize(EncoderStateDeserializer &idata);
};

struct DecoderState
{
  uint16_t width, height;

  ProbabilityTables probability_tables = {};
  Optional<Segmentation> segmentation = {};
  Optional<FilterAdjustments> filter_adjustments = {};

  // testing only
  DecoderState(const unsigned s_width, const unsigned s_height,
               ProbabilityTables &&p, Optional<Segmentation> &&s,
               Optional<FilterAdjustments> &&f);

  DecoderState(EncoderStateDeserializer &idata, const unsigned s_width, const unsigned s_height);

  DecoderState( const unsigned int s_width, const unsigned int s_height );

  DecoderState( const KeyFrameHeader & header,
                const unsigned int s_width,
                const unsigned int s_height );

  template <class FrameType>
  FrameType parse_and_apply( const UncompressedChunk & uncompressed_chunk );

  bool operator==( const DecoderState & other ) const;

  bool operator!=( const DecoderState & other ) const { return not operator==( other ); }

  Optional<ModeRefLFDeltaUpdate> get_filter_update( void ) const;
  Optional<SegmentFeatureData> get_segment_update( void ) const;

  size_t hash( void ) const;

  size_t serialize(EncoderStateSerializer &odata) const;
  static DecoderState deserialize(EncoderStateDeserializer &idata);
};

class DecoderHash
{
private:
  size_t state_hash_, last_hash_, golden_hash_, alt_hash_;
public:
  DecoderHash( size_t state_hash, size_t last_hash,
               size_t golden_hash, size_t alt_hash );

  size_t hash( void ) const;

  std::string str( void ) const;

  bool operator==( const DecoderHash & other ) const;

  bool operator!=( const DecoderHash & other ) const;
};

class Decoder
{
private:
  DecoderState state_;
  References references_;

  bool error_concealment_ { false };

public:
  Decoder( const uint16_t width, const uint16_t height );
  Decoder( DecoderState state, References references );
  Decoder( EncoderStateDeserializer &idata );

  const VP8Raster & example_raster( void ) const { return references_.last; }

  DecoderHash get_hash( void ) const;

  UncompressedChunk decompress_frame( const Chunk & compressed_frame ) const;

  template<class FrameType>
  FrameType parse_frame( const UncompressedChunk & decompressed_frame );

  template<class FrameType>
  std::pair<bool, RasterHandle> decode_frame( const FrameType & frame );

  std::pair<bool, RasterHandle> get_frame_output( const Chunk & compressed_frame );
  Optional<RasterHandle> parse_and_decode_frame( const Chunk & compressed_frame );

  template <class FrameType>
  void apply_decoded_frame( const FrameType & frame, const RasterHandle & output, const Decoder & target )
  {
    state_ = target.state_;
    frame.copy_to( output, references_ );
  }

  DecoderState get_state() const { return state_; }

  References get_references( void ) const { return references_; }

  uint16_t get_width() const { return state_.width; }
  uint16_t get_height() const { return state_.height; }

  bool operator==( const Decoder & other ) const;

  bool operator!=( const Decoder & other ) const { return not operator==( other ); }

  uint32_t minihash() const;

  bool minihash_match( const uint32_t other_minihash ) const;

  size_t serialize(EncoderStateSerializer &odata) const;

  static Decoder deserialize(EncoderStateDeserializer &idata);

  void set_error_concealment( const bool val ) { error_concealment_ = val; }
  bool error_concealment() const { return error_concealment_; }
};


#endif /* DECODER_HH */
