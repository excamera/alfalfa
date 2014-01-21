#ifndef BLOCK_HH
#define BLOCK_HH

#include "modemv_data.hh"
#include "2d.hh"
#include "raster.hh"
#include "safe_array.hh"
#include "vp8_header_structures.hh"

class ProbabilityTables;
class Quantizer;
class BoolEncoder;

enum BlockType { Y_after_Y2 = 0, Y2, UV, Y_without_Y2 };

enum class PixelAdjustment : int8_t { MinusOne = -1, NoAdjustment = 0, PlusOne = 1 };

template <BlockType initial_block_type, class PredictionMode>
class Block
{
private:
  typename TwoD< Block >::Context context_;

  BlockType type_ { initial_block_type };

  PredictionMode prediction_mode_ {};

  SafeArray< int16_t, 16 > coefficients_ {{}};
  SafeArray< PixelAdjustment, 16 > pixel_adjustments_ {{}}; /* only used for continuation */

  bool coded_ { true };

  bool has_nonzero_ { false };

  bool has_pixel_adjustment_ { false };

  MotionVector motion_vector_ {};

  Block dequantize_internal( const uint16_t dc_factor, const uint16_t ac_factor ) const;

public:
  Block( const typename TwoD< Block >::Context & context )
    : context_( context )
  {}

  const PredictionMode & prediction_mode( void ) const { return prediction_mode_; }

  void set_prediction_mode( const PredictionMode prediction_mode )
  {
    prediction_mode_ = prediction_mode;
  }

  bool has_pixel_adjustment( void ) const { return has_pixel_adjustment_; }

  void set_has_pixel_adjustment( const bool has_pixel_adjustment )
  {
    has_pixel_adjustment_ = has_pixel_adjustment;
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
    if ( (prediction_mode_ == B_PRED) or (prediction_mode_ == SPLITMV) ) {
      coded_ = false;
    }
  }

  void parse_tokens( BoolDecoder & data, const ProbabilityTables & probability_tables, const bool continuation );

  BlockType type( void ) const { return type_; }
  bool coded( void ) const { static_assert( initial_block_type == Y2,
					    "only Y2 blocks can be omitted" ); return coded_; }
  bool has_nonzero( void ) const { return has_nonzero_; }

  void walsh_transform( TwoD< Block< Y_after_Y2, bmode > > & output ) const;
  void idct_add( Raster::Block4 & output ) const;
  void apply_pixel_adjustment( Raster::Block4 & output ) const;
  void set_dc_coefficient( const int16_t & val );
  Block dequantize( const Quantizer & quantizer ) const;

  const MotionVector & motion_vector( void ) const
  {
    static_assert( initial_block_type != Y2, "Y2 blocks do not have motion vectors" );
    return motion_vector_;
  }

  void set_motion_vector( const MotionVector & other )
  {
    static_assert( initial_block_type != Y2, "Y2 blocks do not have motion vectors" );
    motion_vector_ = other;
  }

  void read_subblock_inter_prediction( BoolDecoder & data, const MotionVector & best_mv,
				       const SafeArray< SafeArray< Probability, MV_PROB_CNT >, 2 > & motion_vector_probs );

  void write_subblock_inter_prediction( BoolEncoder & encoder, const MotionVector & best_mv,
					const SafeArray< SafeArray< Probability, MV_PROB_CNT >, 2 > & motion_vector_probs ) const;

  void serialize_tokens( BoolEncoder & data,
			 const ProbabilityTables & probability_tables,
			 const bool with_pixel_adjustment ) const;

  SafeArray< int16_t, 16 > & mutable_coefficients( void ) { return coefficients_; }
  const SafeArray< int16_t, 16 > & coefficients( void ) const { return coefficients_; }

  SafeArray< PixelAdjustment, 16 > & mutable_pixel_adjustments( void ) { return pixel_adjustments_; }
  const SafeArray< PixelAdjustment, 16 > & pixel_adjustments( void ) const { return pixel_adjustments_; }

  void fdct( const SafeArray< SafeArray< int16_t, 4 >, 4 > & input );

  void idct_add_pixel_adjust( Raster::Block4 & output ) const
  {
    idct_add( output );
    apply_pixel_adjustment( output );
  }
};

using Y2Block = Block< Y2, mbmode >;
using YBlock = Block< Y_after_Y2, bmode >;
using UVBlock = Block< UV, mbmode >;

#endif /* BLOCK_HH */
