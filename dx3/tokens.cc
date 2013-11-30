#include <array>

#include "macroblock_header.hh"
#include "tokens.hh"
#include "bool_decoder.hh"

using namespace std;

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

static const array< uint8_t, 16 > coefficient_to_band {{ 0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7 }};

template <BlockType initial_block_type, class PredictionMode >
void Block< initial_block_type,
	    PredictionMode >::parse_tokens( BoolDecoder & data,
					    const KeyFrameHeader::DerivedQuantities & probability_tables )
{
  if ( not coded_ ) {
    return;
  }

  bool last_was_zero = false;

  Block default_block;

  /* prediction context starts with number-not-zero count */
  char context = above_.get_or( &default_block )->has_nonzero()
    + left_.get_or( &default_block )->has_nonzero();

  for ( unsigned int index = (type() == BlockType::Y_after_Y2) ? 1 : 0;
	index < 16;
	index++ ) {
    const ProbabilityArray< MAX_ENTROPY_TOKENS > & prob
      = probability_tables.coeff_probs.at( type_ ).at( coefficient_to_band.at( index ) ).at( context );
    const token coded_token = data.tree< MAX_ENTROPY_TOKENS,
					 token >( token_tree, prob, last_was_zero ? 2 : 0 );

    /* check for end of block */
    if ( coded_token == DCT_EOB_TOKEN ) {
      break;
    }

    /* update prediction context */
    last_was_zero = (coded_token == ZERO_TOKEN);

    switch ( coded_token ) {
    case ZERO_TOKEN: context = 0; break;
    case ONE_TOKEN: context = 1; break;
    default: context = 2; break;
    };
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
