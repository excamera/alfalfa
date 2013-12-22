#ifndef MB_RECORDS_HH
#define MB_RECORDS_HH

#include "vp8_header_structures.hh"
#include "frame_header.hh"
#include "2d.hh"
#include "block.hh"
#include "raster.hh"

#include "tree.cc"

class KeyFrameMacroblockHeader
{
private:
  Optional< Tree< uint8_t, 4, segment_id_tree > > segment_id_;
  Optional< Bool > mb_skip_coeff_;

  Y2Block & Y2_;
  TwoDSubRange< YBlock > Y_;
  TwoDSubRange< UVBlock > U_, V_;

  Optional< Raster::Macroblock * > raster_ {};

  const intra_mbmode & uv_prediction_mode( void ) const { return U_.at( 0, 0 ).prediction_mode(); }

public:
  KeyFrameMacroblockHeader( const TwoD< KeyFrameMacroblockHeader >::Context & c,
			    BoolDecoder & data,
			    const KeyFrameHeader & key_frame_header,
			    const KeyFrameHeader::DerivedQuantities & probability_tables,
			    TwoD< Y2Block > & frame_Y2,
			    TwoD< YBlock > & frame_Y,
			    TwoD< UVBlock > & frame_U,
			    TwoD< UVBlock > & frame_V );

  void parse_tokens( BoolDecoder & data,
		     const KeyFrameHeader::DerivedQuantities & probability_tables );

  void dequantize( const KeyFrameHeader::DerivedQuantities & derived );

  void assign_output_raster( Raster::Macroblock & raster ) { raster_.initialize( &raster ); }

  void inverse_transform( void );

  void intra_predict( void );
};

#endif /* MB_RECORDS_HH */
