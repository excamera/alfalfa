#include "macroblock.hh"
#include "tokens.hh"
#include "bool_decoder.hh"
#include "safe_array.hh"
#include "decoder.hh"

using namespace std;

static const SafeArray< uint8_t, 16 > coefficient_to_band {{ 0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7 }};

static const SafeArray< uint8_t, 16 > zigzag = {{ 0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15 }};

template < unsigned int length >
struct TokenDecoder
{
private:
  const uint16_t base_value_;
  const SafeArray< Probability, length > bit_probabilities_;

public:
  TokenDecoder( const uint16_t base_value, const SafeArray< Probability, length > & probs )
    : base_value_( base_value ), bit_probabilities_( probs ) {}

  uint16_t decode( BoolDecoder & data ) const
  {
    uint16_t increment = 0;
    for ( uint8_t i = 0; i < length; i++ ) { increment = ( increment << 1 ) + data.get( bit_probabilities_.at( i ) ); }
    return base_value_ + increment;
  }
};

static const TokenDecoder< 2 > token_decoder_1( 7, { 165, 145 } );
static const TokenDecoder< 3 > token_decoder_2( 11, { 173, 148, 140 } );
static const TokenDecoder< 4 > token_decoder_3( 19, { 176, 155, 140, 135 } );
static const TokenDecoder< 5 > token_decoder_4( 35, { 180, 157, 141, 134, 130 } );
static const TokenDecoder< 11 > token_decoder_5( 67, { 254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129 } );

/* The unfolded token decoder is not pretty, but it is considerably faster
   than using a tree decoder */

template < BlockType initial_block_type, class PredictionMode >
void Block< initial_block_type,
	    PredictionMode >::parse_tokens( BoolDecoder & data,
					    const DecoderState & decoder_state )
{
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
      = decoder_state.coeff_probs.at( type_ ).at( coefficient_to_band.at( index ) ).at( context );

    /* decode the token */
    if ( not last_was_zero ) {
      if ( not data.get( prob.at( 0 ) ) ) {
	/* EOB */
	break;
      }
    }

    if ( not data.get( prob.at( 1 ) ) ) {
      last_was_zero = true;
      context = 0;
      continue;
    }

    last_was_zero = false;
    has_nonzero_ = true;

    int16_t value;

    if ( not data.get( prob.at( 2 ) ) ) {
      value = 1;
      context = 1;
    } else {
      context = 2;
      if ( not data.get( prob.at( 3 ) ) ) {
	if ( not data.get( prob.at( 4 ) ) ) {
	  value = 2;
	} else {
	  if ( not data.get( prob.at( 5 ) ) ) {
	    value = 3;
	  } else {
	    value = 4;
	  }
	}
      } else {
	if ( not data.get( prob.at( 6 ) ) ) {
	  if ( not data.get( prob.at( 7 ) ) ) {
	    value = 5 + data.get( 159 );
	  } else {
	    value = token_decoder_1.decode( data );
	  }
	} else {
	  if ( not data.get( prob.at( 8 ) ) ) {
	    if ( not data.get( prob.at( 9 ) ) ) {
	      value = token_decoder_2.decode( data );
	    } else {
	      value = token_decoder_3.decode( data );
	    }
	  } else {
	    if ( not data.get( prob.at( 10 ) ) ) {
	      value = token_decoder_4.decode( data );
	    } else {
	      value = token_decoder_5.decode( data );
	    }
	  }
	}
      }
    }

    /* decode sign bit if absolute value is nonzero */
    if ( data.bit() ) {
      value = -value;
    }

    /* assign to block storage */
    coefficients_.at( zigzag.at( index ) ) = value;
  }
}
