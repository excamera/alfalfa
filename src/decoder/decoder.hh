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
#include "decoder_tracking.hh"
#include "frame_header.hh"

class Chunk;
class Raster;
class UncompressedChunk;
struct KeyFrameHeader;
struct InterFrameHeader;
struct ContinuationState;

struct ProbabilityTables
{
  SafeArray< SafeArray< SafeArray< SafeArray< Probability,
					      ENTROPY_NODES >,
				   PREV_COEF_CONTEXTS >,
			COEF_BANDS >,
	     BLOCK_TYPES > coeff_probs = k_default_coeff_probs;

  ProbabilityArray< num_y_modes > y_mode_probs = k_default_y_mode_probs;
  ProbabilityArray< num_uv_modes > uv_mode_probs = k_default_uv_mode_probs;

  SafeArray< SafeArray< Probability, MV_PROB_CNT >, 2 > motion_vector_probs = k_default_mv_probs;

  ProbabilityTables() {}

  template <class HeaderType>
  void coeff_prob_update( const HeaderType & header );

  template <class HeaderType>
  void update( const HeaderType & header );

  size_t hash( void ) const;

  bool operator==( const ProbabilityTables & other ) const;
};

struct FilterAdjustments
{
  /* Adjustments to the deblocking filter based on the macroblock's reference frame */
  SafeArray< int8_t, num_reference_frames > loopfilter_ref_adjustments {{}};

  /* Adjustments based on the macroblock's prediction mode */
  SafeArray< int8_t, 4 > loopfilter_mode_adjustments {{}};

  FilterAdjustments() {}

  template <class HeaderType>
  FilterAdjustments( const HeaderType & header ) { update( header ); }

  template <class HeaderType>
  void update( const HeaderType & header );

  size_t hash( void ) const;

  bool operator==( const FilterAdjustments & other ) const;
};

struct References
{
  RasterHandle last, golden, alternative_reference;

  References( const uint16_t width, const uint16_t height );

  References( MutableRasterHandle && raster );

  const Raster & at( const reference_frame reference_id ) const
  {
    switch ( reference_id ) {
    case LAST_FRAME: return last;
    case GOLDEN_FRAME: return golden;
    case ALTREF_FRAME: return alternative_reference;
    default: throw LogicError();
    }
  }
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

  template <class HeaderType>
  void update( const HeaderType & header );

  size_t hash( void ) const;

  bool operator==( const Segmentation & other ) const;

  Segmentation( const Segmentation & other );
};

struct DecoderState
{
  uint16_t width, height;

  ProbabilityTables probability_tables = {};
  Optional< Segmentation > segmentation = {};
  Optional< FilterAdjustments > filter_adjustments = {};

  DecoderState( const unsigned int s_width, const unsigned int s_height );

  DecoderState( const KeyFrameHeader & header,
		const unsigned int s_width,
		const unsigned int s_height );

  template <class FrameType>
  FrameType parse_and_apply( const UncompressedChunk & uncompressed_chunk );

  bool operator==( const DecoderState & other ) const;

  ReplacementEntropyHeader get_replacement_probs( const DecoderState & other ) const;
  Optional<ModeRefLFDeltaUpdate> get_filter_update( void ) const;
  Optional<SegmentFeatureData> get_segment_update( void ) const;

  size_t hash( void ) const;
};

struct DecoderDiff
{
  RasterDiff continuation_diff;
  std::function<SourceHash( const DependencyTracker & deps )> get_source_hash;
  std::function<TargetHash( const UpdateTracker & updates, const RasterHandle & output, bool shown )> get_target_hash;
  References source_refs;
  ReplacementEntropyHeader entropy_header;
  ProbabilityTables target_probabilities;

  Optional<ModeRefLFDeltaUpdate> filter_update;
  Optional<SegmentFeatureData> segment_update;
};

class Decoder
{
private:
  DecoderState state_;  
  References references_;
  RasterHandle continuation_raster_;

  void update_continuation( const MutableRasterHandle & raster );

public:
  Decoder( const uint16_t width, const uint16_t height );

  const Raster & example_raster( void ) const { return references_.last; }

  SourceHash source_hash( const DependencyTracker & deps ) const;
  TargetHash target_hash( const UpdateTracker & updates, const RasterHandle & output, bool shown ) const;
  DecoderHash get_hash( void ) const;

  Optional<RasterHandle> decode_frame( const Chunk & frame );

  ContinuationState next_continuation_state( const Chunk & frame );

  void sync_continuation_raster( const Decoder & other );

  bool operator==( const Decoder & other ) const;

  DecoderDiff operator-( const Decoder & other ) const;
};

#endif /* DECODER_HH */
