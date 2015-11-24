#ifndef BLOCK_HH
#define BLOCK_HH

#include "modemv_data.hh"
#include "2d.hh"
#include "vp8_raster.hh"
#include "safe_array.hh"
#include "vp8_header_structures.hh"

struct ProbabilityTables;
struct Quantizer;
class BoolEncoder;

enum BlockType { Y_after_Y2 = 0, Y2, UV, Y_without_Y2 };

class DCTCoefficients
{
private:
  SafeArray< int16_t, 16 > coefficients_ {{}};
public:

  void reinitialize( void )
  {
    coefficients_ = SafeArray< int16_t, 16 > {{}};
  }

  int16_t & at( const unsigned int index )
  {
    return coefficients_.at( index );
  }

  const int16_t & at( const unsigned int index ) const
  {
    return coefficients_.at( index );
  }

  void walsh_transform( SafeArray< SafeArray< DCTCoefficients, 4 >, 4 > & output ) const;
  void idct_add( VP8Raster::Block4 & output ) const;

  void set_dc_coefficient( const int16_t & val );

  bool operator==( const DCTCoefficients & other ) const
  {
    return coefficients_ == other.coefficients_;
  }

  bool operator!=( const DCTCoefficients & other ) const
  {
    return not operator==( other );
  }
} ;

template <BlockType initial_block_type, class PredictionMode>
class Block
{
private:
  // Keeping coefficients at base address of block reduces pointer arithmetic in accesses
  DCTCoefficients coefficients_ {};

  typename TwoD< Block >::Context context_;

  BlockType type_ { initial_block_type };

  PredictionMode prediction_mode_ {};

  bool coded_ { true };

  bool has_nonzero_ { false };

  MotionVector motion_vector_ {};

  DCTCoefficients dequantize_internal( const uint16_t dc_factor, const uint16_t ac_factor ) const;

public:
  Block( const typename TwoD< Block >::Context & context )
    : context_( context )
  {}

  const PredictionMode & prediction_mode( void ) const { return prediction_mode_; }

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

  void set_coded ( const bool coded )
  {
    coded_ = coded;
  }

  void set_if_coded( void )
  {
    static_assert( initial_block_type == Y2,
		   "set_if_coded attempted on non-Y2 coded block" );
    if ( (prediction_mode_ == B_PRED) or (prediction_mode_ == SPLITMV) ) {
      coded_ = false;
    }
  }

  void parse_tokens( BoolDecoder & data, const ProbabilityTables & probability_tables );

  BlockType type( void ) const { return type_; }
  bool coded( void ) const { static_assert( initial_block_type == Y2,
					    "only Y2 blocks can be omitted" ); return coded_; }
  bool has_nonzero( void ) const { return has_nonzero_; }

  void set_dc_coefficient( const int16_t & val );
  DCTCoefficients dequantize( const Quantizer & quantizer ) const;

  const MotionVector & motion_vector() const
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
			 const ProbabilityTables & probability_tables ) const;

  DCTCoefficients & mutable_coefficients( void ) { return coefficients_; }
  const DCTCoefficients & coefficients( void ) const { return coefficients_; }

  void add_residue( VP8Raster::Block4 & raster ) const;

  void calculate_has_nonzero();

  void zero_out();

  bool operator==( const Block & other ) const
  {
    return coefficients_ == other.coefficients_ and
           prediction_mode_ == other.prediction_mode_ and
           motion_vector_ == other.motion_vector_;
  }

  bool operator!=( const Block & other ) const { return not operator==( other ); }
};

using Y2Block = Block< Y2, mbmode >;
using YBlock = Block< Y_after_Y2, bmode >;
using UVBlock = Block< UV, mbmode >;

#endif /* BLOCK_HH */
