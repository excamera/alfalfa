#ifndef BLOCK_HH
#define BLOCK_HH

#include "modemv_data.hh"
#include "2d.hh"
#include "tokens.hh"
#include "frame_header.hh"

enum class BlockType { Y_after_Y2 = 0, Y2, UV, Y_without_Y2 };

template <BlockType initial_block_type, class PredictionMode>
class Block
{
private:
  BlockType type_ { initial_block_type };

  PredictionMode prediction_mode_ {};
  Optional< Block * > above_ {};
  Optional< Block * > left_ {};

  std::vector< token > tokens_ {};

  bool coded_ { true };

public:
  Block() {}

  Block( typename TwoD< Block >::Context & context )
    : above_( context.above ), left_( context.left )
  {}

  const PredictionMode & prediction_mode( void ) const { return prediction_mode_; }
  void set_prediction_mode( const PredictionMode & prediction_mode )
  {
    prediction_mode_ = prediction_mode;
  }

  const Optional< Block * > above( void ) const { return above_; }
  const Optional< Block * > left( void ) const { return left_; }

  void set_above( const Optional< Block * > & s_above ) { above_ = s_above; }
  void set_left( const Optional< Block * > & s_left ) { left_ = s_left; }

  void set_Y_without_Y2( void )
  {
    static_assert( initial_block_type == BlockType::Y_after_Y2,
		   "set_Y_without_Y2 called on non-Y coded block" );
    type_ = BlockType::Y_without_Y2;
  }

  void set_if_coded( void )
  {
    static_assert( initial_block_type == BlockType::Y2,
		   "set_if_coded called on non-Y2 coded block" );
    if ( prediction_mode_ == B_PRED ) {
      coded_ = false;
    }
  }

  void parse_tokens( BoolDecoder & data,
		     const KeyFrameHeader::DerivedQuantities & probability_tables );

  BlockType type( void ) const { return type_; }
  bool coded( void ) const { return coded_; }
};

using Y2Block = Block< BlockType::Y2, intra_mbmode >;
using YBlock = Block< BlockType::Y_after_Y2, intra_bmode >;
using UBlock = Block< BlockType::UV, intra_mbmode >;
using VBlock = Block< BlockType::UV, intra_mbmode >;

#endif /* BLOCK_HH */
