#ifndef BLOCK_HH
#define BLOCK_HH

#include "modemv_data.hh"
#include "2d.hh"
#include "frame_header.hh"
#include "raster.hh"
#include "quantization.hh"

enum BlockType { Y_after_Y2 = 0, Y2, UV, Y_without_Y2 };

template <BlockType initial_block_type, class PredictionMode>
class Block
{
private:
  BlockType type_ { initial_block_type };

  PredictionMode prediction_mode_ {};
  Optional< const Block * > above_ {};
  Optional< const Block * > left_ {};

  std::array< int16_t, 16 > coefficients_ {{}};

  bool coded_ { true };

  bool has_nonzero_ { false };

public:
  Block() {}

  Block( const typename TwoD< Block >::Context & context )
    : above_( context.above ), left_( context.left )
  {}

  const PredictionMode & prediction_mode( void ) const { return prediction_mode_; }
  void set_prediction_mode( const PredictionMode & prediction_mode )
  {
    prediction_mode_ = prediction_mode;
  }

  const Optional< const Block * > & above( void ) const { return above_; }
  const Optional< const Block * > & left( void ) const { return left_; }

  void set_above( const Optional< const Block * > & s_above ) { above_ = s_above; }
  void set_left( const Optional< const Block * > & s_left ) { left_ = s_left; }

  void set_Y_without_Y2( void )
  {
    static_assert( initial_block_type == Y_after_Y2,
		   "set_Y_without_Y2 called on non-Y coded block" );
    type_ = Y_without_Y2;
  }

  void set_if_coded( void )
  {
    static_assert( initial_block_type == Y2,
		   "set_if_coded attempted on non-Y2 coded block" );
    if ( prediction_mode_ == B_PRED ) {
      coded_ = false;
    }
  }

  void parse_tokens( BoolDecoder & data,
		     const KeyFrameHeader::DerivedQuantities & probability_tables );

  BlockType type( void ) const { return type_; }
  bool coded( void ) const { return coded_; }
  bool has_nonzero( void ) const { return has_nonzero_; }

  void walsh_transform( TwoDSubRange< Block< Y_after_Y2, intra_bmode > > & output );
  void idct( Raster::Block4 & output );
  void set_dc_coefficient( const int16_t & val );
  void dequantize( const Quantizer & quantizer );
};

using Y2Block = Block< Y2, intra_mbmode >;
using YBlock = Block< Y_after_Y2, intra_bmode >;
using UVBlock = Block< UV, intra_mbmode >;

#endif /* BLOCK_HH */
