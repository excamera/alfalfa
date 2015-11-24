#ifndef MB_RECORDS_HH
#define MB_RECORDS_HH

#include "vp8_header_structures.hh"
#include "frame_header.hh"
#include "2d.hh"
#include "block.hh"
#include "vp8_raster.hh"
#include "decoder.hh"

struct ProbabilityTables;
struct References;
class BoolEncoder;
class ReferenceUpdater;

typedef SafeArray< SafeArray< SafeArray< SafeArray< std::pair< uint32_t, uint32_t >,
						    ENTROPY_NODES >,
					 PREV_COEF_CONTEXTS >,
			      COEF_BANDS >,
		   BLOCK_TYPES > TokenBranchCounts;

template <class FrameHeaderType, class MacroblockHeaderType>
class Macroblock
{
private:
  typename TwoD< Macroblock >::Context context_;

  Optional< Tree< uint8_t, num_segments, segment_id_tree > > segment_id_update_;
  uint8_t segment_id_;

  Optional< Boolean > mb_skip_coeff_;

  MacroblockHeaderType header_;

  Y2Block & Y2_;
  TwoDSubRange< YBlock, 4, 4 > Y_;
  TwoDSubRange< UVBlock, 2, 2 > U_, V_;

  bool has_nonzero_ { false };

  void decode_prediction_modes( BoolDecoder & data,
				const ProbabilityTables & probability_tables );

  void encode_prediction_modes( BoolEncoder & encoder,
				const ProbabilityTables & probability_tables ) const;

  void set_base_motion_vector( const MotionVector & mv );

  void apply_walsh( const Quantizer & quantizer, VP8Raster::Macroblock & raster ) const;

public:
  Macroblock( const typename TwoD< Macroblock >::Context & c,
	      BoolDecoder & data,
	      const FrameHeaderType & key_frame_header,
	      const ProbabilityArray< num_segments > & mb_segment_tree_probs,
	      const ProbabilityTables & probability_tables,
	      TwoD< Y2Block > & frame_Y2,
	      TwoD< YBlock > & frame_Y,
	      TwoD< UVBlock > & frame_U,
	      TwoD< UVBlock > & frame_V );

  /* Partial Reference Update */
  Macroblock( const typename TwoD< Macroblock >::Context & c,
              const ReferenceUpdater & ref_update,
	      TwoD< Y2Block > & frame_Y2,
	      TwoD< YBlock > & frame_Y,
	      TwoD< UVBlock > & frame_U,
	      TwoD< UVBlock > & frame_V );

  /* Copy */
  Macroblock( const typename TwoD< Macroblock >::Context & c,
            const TwoD< Macroblock > & old_macroblocks,
            TwoD< Y2Block > & frame_Y2,
            TwoD< YBlock > & frame_Y,
            TwoD< UVBlock > & frame_U,
            TwoD< UVBlock > & frame_V );

  void update_segmentation( SegmentationMap & mutable_segmentation_map );

  void parse_tokens( BoolDecoder & data,
		     const ProbabilityTables & probability_tables );

  void reconstruct_intra( const Quantizer & quantizer, VP8Raster::Macroblock & raster ) const;
  void reconstruct_inter( const Quantizer & quantizer,
			  const References & references,
			  VP8Raster::Macroblock & raster ) const;
  void reconstruct_continuation( const VP8Raster & continuation, VP8Raster::Macroblock & raster ) const;

  /* list of coordinates for macroblocks that inter prediction needs */
  void record_dependencies( std::vector<std::vector<bool>> & required ) const;

  void loopfilter( const Optional< FilterAdjustments > & filter_adjustments,
		   const FilterParameters & loopfilter,
		   VP8Raster::Macroblock & raster ) const;

  const MacroblockHeaderType & header( void ) const { return header_; }
  const MotionVector & base_motion_vector( void ) const;

  const mbmode & uv_prediction_mode( void ) const { return U_.at( 0, 0 ).prediction_mode(); }
  const mbmode & y_prediction_mode( void ) const { return Y2_.prediction_mode(); }

  bool inter_coded() const;

  Optional< Tree< uint8_t, num_segments, segment_id_tree > > & mutable_segment_id_update( void ) { return segment_id_update_; }
  uint8_t segment_id() const { return segment_id_; }

  void serialize( BoolEncoder & encoder,
		  const FrameHeaderType & frame_header,
		  const ProbabilityArray< num_segments > & mb_segment_tree_probs,
		  const ProbabilityTables & probability_tables ) const;

  void serialize_tokens( BoolEncoder & encoder,
			 const ProbabilityTables & probability_tables ) const;

  void accumulate_token_branches( TokenBranchCounts & counts ) const;

  void zero_out();

  void analyze_dependencies( VP8Raster::Macroblock & raster, const VP8Raster & reference ) const;

  bool operator==( const Macroblock & other ) const { return has_nonzero_ == other.has_nonzero_ and mb_skip_coeff_ == other.mb_skip_coeff_ and segment_id_ == other.segment_id_; }

  bool operator!=( const Macroblock & other ) const { return not operator==( other ); }
};

struct KeyFrameMacroblockHeader
{
  /* no fields beyond what's in every macroblock */

  KeyFrameMacroblockHeader( BoolDecoder &, const KeyFrameHeader & ) {}

  reference_frame reference( void ) const { return CURRENT_FRAME; }
};

struct InterFrameMacroblockHeader
{
  Boolean is_inter_mb;
  Optional< Boolean > mb_ref_frame_sel1;
  Optional< Boolean > mb_ref_frame_sel2;

  bool motion_vectors_flipped_; /* derived quantity */

  Optional< Tree< uint8_t, 4, split_mv_tree > > partition_id {};

  InterFrameMacroblockHeader( BoolDecoder & data, const InterFrameHeader & frame_header );

  reference_frame reference( void ) const;
};

/* State update frames don't actually have macroblocks, so this does nothing */
struct StateUpdateFrameMacroblockHeader
{
  StateUpdateFrameMacroblockHeader( BoolDecoder &, const StateUpdateFrameHeader & ) {}
  StateUpdateFrameMacroblockHeader() {}
};

struct RefUpdateFrameMacroblockHeader
{
  RefUpdateFrameMacroblockHeader( BoolDecoder &, const RefUpdateFrameHeader &) {}
  RefUpdateFrameMacroblockHeader() {}
};

using KeyFrameMacroblock = Macroblock<KeyFrameHeader, KeyFrameMacroblockHeader>;
using InterFrameMacroblock = Macroblock<InterFrameHeader, InterFrameMacroblockHeader>;
using StateUpdateFrameMacroblock = Macroblock<StateUpdateFrameHeader, StateUpdateFrameMacroblockHeader>;
using RefUpdateFrameMacroblock = Macroblock<RefUpdateFrameHeader, RefUpdateFrameMacroblockHeader>;

#endif /* MB_RECORDS_HH */
