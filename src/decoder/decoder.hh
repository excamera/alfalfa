#ifndef DECODER_HH
#define DECODER_HH

#include "safe_array.hh"
#include "modemv_data.hh"
#include "loopfilter.hh"
#include "vp8_prob_data.hh"
#include "quantization.hh"

class Chunk;
class Raster;

struct DecoderState
{
  ProbabilityArray< num_segments > mb_segment_tree_probs;
  SafeArray< SafeArray< SafeArray< SafeArray< Probability,
					      ENTROPY_NODES >,
				   PREV_COEF_CONTEXTS >,
			COEF_BANDS >,
	     BLOCK_TYPES > coeff_probs;

  Quantizer quantizer;
  SafeArray< Quantizer, num_segments > segment_quantizers;

  FilterParameters loop_filter;
  SafeArray< FilterParameters, num_segments > segment_loop_filters;

  SafeArray< int8_t, num_reference_frames > loopfilter_ref_adjustments;
  SafeArray< int8_t, 4 > loopfilter_mode_adjustments;

  ProbabilityArray< num_y_modes > y_mode_probs;
  ProbabilityArray< num_intra_b_modes > b_mode_probs;
  ProbabilityArray< num_uv_modes > uv_mode_probs;

  DecoderState( const KeyFrameHeader & header );
};

class Decoder
{
private:
  uint16_t width_, height_;

  DecoderState state_;

public:
  Decoder( uint16_t s_width, uint16_t s_height, const Chunk & key_frame );

  bool decode_frame( const Chunk & frame, Raster & raster );
};

#endif /* DECODER_HH */
