#ifndef MB_RECORDS_HH
#define MB_RECORDS_HH

#include "vp8_header_structures.hh"
#include "frame_header.hh"
#include "2d.hh"
#include "block.hh"
#include "raster.hh"
#include "decoder.hh"

class QuantizerFilterAdjustments;
class ProbabilityTables;
class References;
class BoolEncoder;

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

public:
  Macroblock( const typename TwoD< Macroblock >::Context & c,
	      BoolDecoder & data,
	      const FrameHeaderType & key_frame_header,
	      const ProbabilityArray< num_segments > & mb_segment_tree_probs,
	      SegmentationMap & mutable_segmentation_map,
	      const ProbabilityTables & probability_tables,
	      TwoD< Y2Block > & frame_Y2,
	      TwoD< YBlock > & frame_Y,
	      TwoD< UVBlock > & frame_U,
	      TwoD< UVBlock > & frame_V );

  void parse_tokens( BoolDecoder & data, const ProbabilityTables & probability_tables );

  void reconstruct_intra( const Quantizer & quantizer, Raster::Macroblock & raster ) const;
  void reconstruct_inter( const Quantizer & quantizer,
			  const References & references,
			  Raster::Macroblock & raster ) const;

  void rewrite_as_intra( Raster::Macroblock & raster );

  void loopfilter( const QuantizerFilterAdjustments & quantizer_filter_adjustments,
		   const bool adjust_for_mode_and_ref,
		   const FilterParameters & loopfilter,
		   Raster::Macroblock & raster ) const;

  const MacroblockHeaderType & header( void ) const { return header_; }
  const MotionVector & base_motion_vector( void ) const;

  const mbmode & uv_prediction_mode( void ) const { return U_.at( 0, 0 ).prediction_mode(); }
  const mbmode & y_prediction_mode( void ) const { return Y2_.prediction_mode(); }

  bool inter_coded( void ) const;

  uint8_t segment_id( void ) const { return segment_id_; }

  void serialize( BoolEncoder & encoder,
		  const FrameHeaderType & frame_header,
		  const ProbabilityArray< num_segments > & mb_segment_tree_probs,
		  const ProbabilityTables & probability_tables ) const;

  void serialize_tokens( BoolEncoder & encoder, const ProbabilityTables & probability_tables ) const;
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

using KeyFrameMacroblock = Macroblock< KeyFrameHeader, KeyFrameMacroblockHeader >;
using InterFrameMacroblock = Macroblock< InterFrameHeader, InterFrameMacroblockHeader >;

#endif /* MB_RECORDS_HH */
