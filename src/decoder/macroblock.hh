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
  MacroblockHeaderType header_;

  Y2Block & Y2_;
  TwoDSubRange< YBlock, 4, 4 > Y_;
  TwoDSubRange< UVBlock, 2, 2 > U_, V_;

  const intra_mbmode & uv_prediction_mode( void ) const { return U_.at( 0, 0 ).prediction_mode(); }

  bool has_nonzero_ { false };

  void decode_prediction_modes( BoolDecoder & data );

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

  void dequantize( const DecoderState & decoder_state );

  void intra_predict_and_inverse_transform( Raster::Macroblock & raster ) const;

  void loopfilter( const DecoderState & decoder_state, Raster::Macroblock & raster ) const;  
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
  Optional< Tree< uint8_t, 4, segment_id_tree > > segment_id;
  Optional< Boolean > mb_skip_coeff;

  InterFrameMacroblockHeader( BoolDecoder & data,
			      const InterFrameHeader & frame_header,
			      const DecoderState & decoder_state );
};

using KeyFrameMacroblock = Macroblock< KeyFrameHeader, KeyFrameMacroblockHeader >;
using InterFrameMacroblock = Macroblock< InterFrameHeader, InterFrameMacroblockHeader >;

#endif /* MB_RECORDS_HH */
