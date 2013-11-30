#include "macroblock_header.hh"

void KeyFrameMacroblockHeader::parse_tokens( BoolDecoder & data,
					     const KeyFrameHeader::DerivedQuantities & probability_tables )
{
  /* is macroblock skipped? */
  if ( mb_skip_coeff_.get_or( false ) ) {
    return;
  }

  /* parse Y2 block if present */
  Y2_.parse_tokens( data, probability_tables );

  /* parse Y blocks with variable first coefficient */
  Y_.forall( [&]( YBlock & block ) { block.parse_tokens( data, probability_tables ); } );

  /* parse U and V blocks */
  U_.forall( [&]( UBlock & block ) { block.parse_tokens( data, probability_tables ); } );
  V_.forall( [&]( VBlock & block ) { block.parse_tokens( data, probability_tables ); } );
}

template <BlockType initial_block_type, class PredictionMode >
void Block< initial_block_type,
	    PredictionMode >::parse_tokens( BoolDecoder & ,
					    const KeyFrameHeader::DerivedQuantities &  )
{
  if ( not coded_ ) {
    return;
  }

  
}

const TreeArray< MAX_ENTROPY_TOKENS > token_tree = {{
    -DCT_EOB_TOKEN, 2,
    -ZERO_TOKEN, 4,
    -ONE_TOKEN, 6,
    8, 12,
    -TWO_TOKEN, 10,
    -THREE_TOKEN, -FOUR_TOKEN,
    14, 16,
    -DCT_VAL_CATEGORY1, -DCT_VAL_CATEGORY2,
    18, 20,
    -DCT_VAL_CATEGORY3, -DCT_VAL_CATEGORY4,
    -DCT_VAL_CATEGORY5, -DCT_VAL_CATEGORY6
  }};
