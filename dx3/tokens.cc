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

static const array< uint8_t, 16 > zigzag = {{ 0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15 }};

struct TokenDecoder
{
private:
  const uint16_t base_value_;
  const vector< Probability > bit_probabilities_;

public:
  TokenDecoder( const uint16_t base_value, const vector< Probability > probs )
    : base_value_( base_value ), bit_probabilities_( probs ) {}

  uint16_t decode( BoolDecoder & data ) const
  {
    uint16_t increment = 0;
    for ( auto & x : bit_probabilities_ ) { increment += increment + data.get( x ); }
    return base_value_ + increment;
  }
};

static const array< TokenDecoder, ENTROPY_NODES > token_decoders = {{
    TokenDecoder( 0, {} ),
    TokenDecoder( 1, {} ),
    TokenDecoder( 2, {} ),
    TokenDecoder( 3, {} ),
    TokenDecoder( 4, {} ),
    TokenDecoder( 5, { 159 } ), /* range 5..6 */
    TokenDecoder( 7, { 165, 145 } ), /* range 7..10 */
    TokenDecoder( 11, { 173, 148, 140 } ),
    TokenDecoder( 19, { 176, 155, 140, 135 } ),
    TokenDecoder( 35, { 180, 157, 141, 134, 130 } ),
    TokenDecoder( 67, { 254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129 } )
  }};

template < BlockType initial_block_type, class PredictionMode >
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

  for ( unsigned int index = (type_ == BlockType::Y_after_Y2) ? 1 : 0;
	index < 16;
	index++ ) {
    /* select the tree probabilities based on the prediction context */
    const ProbabilityArray< MAX_ENTROPY_TOKENS > & prob
      = probability_tables.coeff_probs.at( type_ ).at( coefficient_to_band.at( index ) ).at( context );

    /* decode the token */
    const token coded_token = data.tree< MAX_ENTROPY_TOKENS,
					 token >( token_tree, prob, last_was_zero ? 2 : 0 );

    /* check for end of block */
    if ( coded_token == DCT_EOB_TOKEN ) {
      break;
    }

    /* update the block's record of whether it has a nonzero coefficient */
    has_nonzero_ |= (coded_token != ZERO_TOKEN);

    /* update prediction context */
    last_was_zero = (coded_token == ZERO_TOKEN);

    /* after first-decoded coefficient, context becomes summary of magnitude of last coefficient */
    switch ( coded_token ) {
    case ZERO_TOKEN: context = 0; break;
    case ONE_TOKEN:  context = 1; break;
    default:         context = 2; break;
    };

    /* decode extra bits if there are any */
    int16_t coefficient = token_decoders.at( coded_token ).decode( data );

    /* decode sign bit if absolute value is nonzero */
    if ( (coded_token > ZERO_TOKEN) and data.bit() ) {
      coefficient = -coefficient;
    }

    /* assign to block storage */
    coefficients_.at( zigzag.at( index ) ) = coefficient;
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
