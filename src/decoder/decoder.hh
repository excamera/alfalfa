#ifndef DECODER_HH
#define DECODER_HH

#include "safe_array.hh"
#include "modemv_data.hh"
#include "loopfilter.hh"
#include "vp8_prob_data.hh"
#include "quantization.hh"

class Chunk;
class Raster;
struct KeyFrameHeader;
struct InterFrameHeader;

struct ProbabilityTables
{
  SafeArray< SafeArray< SafeArray< SafeArray< Probability,
					      ENTROPY_NODES >,
				   PREV_COEF_CONTEXTS >,
			COEF_BANDS >,
	     BLOCK_TYPES > coeff_probs;

  ProbabilityArray< num_y_modes > y_mode_probs;
  ProbabilityArray< num_uv_modes > uv_mode_probs;

  SafeArray< SafeArray< Probability, MV_PROB_CNT >, 2 > motion_vector_probs;

  ProbabilityTables( const KeyFrameHeader & header );

  template <class HeaderType>
  void coeff_prob_update( const HeaderType & header );

  void update( const InterFrameHeader & header );
};

struct QuantizerFilterAdjustments
{
  /* Whether segment-based adjustments are absolute or relative */
  bool absolute_segment_adjustments;

  /* Segment-based adjustments to the quantizer */
  SafeArray< int8_t, num_segments > segment_quantizer_adjustments;

  /* Segment-based adjustments to the in-loop deblocking filter */
  SafeArray< int8_t, num_segments > segment_filter_adjustments;

  /* Adjustments to the deblocking filter based on the macroblock's reference frame */
  SafeArray< int8_t, num_reference_frames > loopfilter_ref_adjustments;

  /* Adjustments based on the macroblock's prediction mode */
  SafeArray< int8_t, 4 > loopfilter_mode_adjustments;

  QuantizerFilterAdjustments( const KeyFrameHeader & header );

  template <class HeaderType>
  void update( const HeaderType & header );
};

struct References
{
  Raster last, golden, alternative_reference;

  References( const uint16_t width, const uint16_t height );

  const Raster & at( const reference_frame reference_id ) const
  {
    switch ( reference_id ) {
    case LAST_FRAME: return last;
    case GOLDEN_FRAME: return golden;
    case ALTREF_FRAME: return alternative_reference;
    default: assert( false ); return last;
    }
  }
};

struct SegmentationMap
{
  /* Probability table to select macroblock segment_id */
  ProbabilityArray< num_segments > mb_segment_tree_probs;

  /* Map of which segment each macroblock belongs to */
  TwoD< uint8_t > map;

  SegmentationMap( const KeyFrameHeader & header,
		   const unsigned int macroblock_width,
		   const unsigned int macroblock_height );

  template <class HeaderType>
  void update( const HeaderType & header );
};

struct DecoderState
{
  QuantizerFilterAdjustments quantizer_filter_adjustments;  
  ProbabilityTables probability_tables;
  SegmentationMap segmentation_map;

  DecoderState( const KeyFrameHeader & header, const unsigned int width, const unsigned int height );
};

class Decoder
{
private:
  uint16_t width_, height_;

  DecoderState state_;  
  References references_;

public:
  Decoder( const uint16_t width, const uint16_t height, const Chunk & key_frame );

  bool decode_frame( const Chunk & frame, Raster & raster );
};

#endif /* DECODER_HH */
