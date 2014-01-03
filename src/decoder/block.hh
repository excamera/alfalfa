#ifndef BLOCK_HH
#define BLOCK_HH

#include "modemv_data.hh"
#include "2d.hh"
#include "frame_header.hh"
#include "raster.hh"
#include "quantization.hh"
#include "safe_array.hh"

class DecoderState;

enum BlockType { Y_after_Y2 = 0, Y2, UV, Y_without_Y2 };

template <BlockType initial_block_type, class PredictionDecoder>
class Block
{
private:
  typename TwoD< Block >::Context context_;

  BlockType type_ { initial_block_type };

  typedef typename PredictionDecoder::type PredictionMode;

  PredictionMode prediction_mode_ {};

  SafeArray< int16_t, 16 > coefficients_ {{}};

  bool coded_ { true };

  bool has_nonzero_ { false };

public:
  Block( const typename TwoD< Block >::Context & context )
    : context_( context )
  {}

  const PredictionMode & prediction_mode( void ) const { return prediction_mode_; }

  void decode_prediction_mode( BoolDecoder & data,
			       const typename PredictionDecoder::probability_array_type & probabilities )
  {
    prediction_mode_ = PredictionDecoder( data, probabilities );
  }

  void set_prediction_mode( const PredictionMode prediction_mode )
  {
    prediction_mode_ = prediction_mode;
  }

  const typename TwoD< Block >::Context & context( void ) const { return context_; }

  void set_above( const Optional< const Block * > & s_above ) { context_.above = s_above; }
  void set_left( const Optional< const Block * > & s_left ) { context_.left = s_left; }

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

  void parse_tokens( BoolDecoder & data, const DecoderState & decoder );

  BlockType type( void ) const { return type_; }
  bool coded( void ) const { return coded_; }
  bool has_nonzero( void ) const { return has_nonzero_; }

  void walsh_transform( TwoDSubRange< Block< Y_after_Y2, Tree< bmode, num_intra_b_modes, b_mode_tree > >, 4, 4 > & output ) const;
  void idct( Raster::Block4 & output ) const;
  void set_dc_coefficient( const int16_t & val );
  void dequantize( const Quantizer & quantizer );
};

using Y2Block = Block< Y2, Tree< mbmode, num_y_modes, kf_y_mode_tree > >;
using YBlock = Block< Y_after_Y2, Tree< bmode, num_intra_b_modes, b_mode_tree > >;
using UVBlock = Block< UV, Tree< mbmode, num_uv_modes, uv_mode_tree > >;

#endif /* BLOCK_HH */
