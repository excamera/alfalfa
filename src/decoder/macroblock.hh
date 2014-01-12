#ifndef MB_RECORDS_HH
#define MB_RECORDS_HH

#include "vp8_header_structures.hh"
#include "frame_header.hh"
#include "2d.hh"
#include "block.hh"
#include "raster.hh"

class QuantizerFilterAdjustments;
class ProbabilityTables;
class SegmentationMap;
class References;

template <class FrameHeaderType, class MacroblockHeaderType>
class Macroblock
{
private:
  typename TwoD< Macroblock >::Context context_;

  Optional< Tree< uint8_t, 4, segment_id_tree > > segment_id_update_;
  uint8_t segment_id_;

  Optional< Boolean > mb_skip_coeff_;

  MacroblockHeaderType header_;

  Y2Block & Y2_;
  TwoDSubRange< YBlock, 4, 4 > Y_;
  TwoDSubRange< UVBlock, 2, 2 > U_, V_;

  bool has_nonzero_ { false };

  void decode_prediction_modes( BoolDecoder & data,
				const ProbabilityTables & probability_tables );

  void set_base_motion_vector( const MotionVector & mv );

public:
  Macroblock( const typename TwoD< Macroblock >::Context & c,
	      BoolDecoder & data,
	      const FrameHeaderType & key_frame_header,
	      SegmentationMap & segmentation_map,
	      const ProbabilityTables & probability_tables,
	      TwoD< Y2Block > & frame_Y2,
	      TwoD< YBlock > & frame_Y,
	      TwoD< UVBlock > & frame_U,
	      TwoD< UVBlock > & frame_V );

  void parse_tokens( BoolDecoder & data, const ProbabilityTables & probability_tables );

  void dequantize( const Quantizer & quantizer );

  void intra_predict_and_inverse_transform( Raster::Macroblock & raster ) const;
  void inter_predict_and_inverse_transform( const References & references,
					    Raster::Macroblock & raster ) const;

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

  InterFrameMacroblockHeader( BoolDecoder & data, const InterFrameHeader & frame_header );

  reference_frame reference( void ) const;
};

using KeyFrameMacroblock = Macroblock< KeyFrameHeader, KeyFrameMacroblockHeader >;
using InterFrameMacroblock = Macroblock< InterFrameHeader, InterFrameMacroblockHeader >;

#endif /* MB_RECORDS_HH */
