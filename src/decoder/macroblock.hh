#ifndef MB_RECORDS_HH
#define MB_RECORDS_HH

#include "vp8_header_structures.hh"
#include "frame_header.hh"
#include "2d.hh"
#include "block.hh"
#include "raster.hh"

class DecoderState;

template <class FrameHeaderType, class MacroblockHeaderType>
class Macroblock
{
private:
  typename TwoD< Macroblock >::Context context_;

  MacroblockHeaderType header_;

  Y2Block & Y2_;
  TwoDSubRange< YBlock, 4, 4 > Y_;
  TwoDSubRange< UVBlock, 2, 2 > U_, V_;

  bool has_nonzero_ { false };

  void decode_prediction_modes( BoolDecoder & data,
				const DecoderState & decoder_state,
				const FrameHeaderType & frame_header );

  void set_base_motion_vector( const MotionVector & mv );

public:
  Macroblock( const typename TwoD< Macroblock >::Context & c,
	      BoolDecoder & data,
	      const FrameHeaderType & key_frame_header,
	      const DecoderState & decoder_state,
	      TwoD< Y2Block > & frame_Y2,
	      TwoD< YBlock > & frame_Y,
	      TwoD< UVBlock > & frame_U,
	      TwoD< UVBlock > & frame_V );

  void parse_tokens( BoolDecoder & data, const DecoderState & decoder_state );

  void dequantize( const Quantizer & frame_quantizer,
		   const SafeArray< Quantizer, num_segments > & segment_quantizers );

  void predict_and_inverse_transform( Raster::Macroblock & raster ) const;

  void loopfilter( const DecoderState & decoder_state,
		   const FilterParameters & frame_loopfilter,
		   const SafeArray< FilterParameters, num_segments > & segment_loopfilters,
		   Raster::Macroblock & raster ) const;

  const MacroblockHeaderType & header( void ) const { return header_; }
  const MotionVector & base_motion_vector( void ) const;

  const mbmode & uv_prediction_mode( void ) const { return U_.at( 0, 0 ).prediction_mode(); }
  const mbmode & y_prediction_mode( void ) const { return Y2_.prediction_mode(); }

  bool inter_coded( void ) const;
};

struct KeyFrameMacroblockHeader
{
  Optional< Tree< uint8_t, 4, segment_id_tree > > segment_id;
  Optional< Boolean > mb_skip_coeff;

  KeyFrameMacroblockHeader( BoolDecoder & data,
			    const KeyFrameHeader & frame_header,
			    const DecoderState & decoder_state );
};

struct InterFrameMacroblockHeader
{
  Optional< Tree< uint8_t, num_segments, segment_id_tree > > segment_id;
  Optional< Boolean > mb_skip_coeff;
  Boolean is_inter_mb;
  Optional< Boolean > mb_ref_frame_sel1;
  Optional< Boolean > mb_ref_frame_sel2;

  bool motion_vectors_flipped_; /* derived quantity */

  InterFrameMacroblockHeader( BoolDecoder & data,
			      const InterFrameHeader & frame_header,
			      const DecoderState & decoder_state );

  reference_frame reference( void ) const;
};

using KeyFrameMacroblock = Macroblock< KeyFrameHeader, KeyFrameMacroblockHeader >;
using InterFrameMacroblock = Macroblock< InterFrameHeader, InterFrameMacroblockHeader >;

#endif /* MB_RECORDS_HH */
