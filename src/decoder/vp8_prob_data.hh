#ifndef VP8_PROB_DATA_HH
#define VP8_PROB_DATA_HH

#include "safe_array.hh"
#include "bool_decoder.hh"
#include "tokens.hh"

const unsigned int BLOCK_TYPES         = 4;
const unsigned int COEF_BANDS         = 8;
const unsigned int PREV_COEF_CONTEXTS = 3;

const unsigned int MV_PROB_CNT = 2 + 8 - 1 + 10;

const extern SafeArray< SafeArray< SafeArray< SafeArray< Probability,
					 ENTROPY_NODES >,
				  PREV_COEF_CONTEXTS >,
			   COEF_BANDS >,
		    BLOCK_TYPES > k_coeff_entropy_update_probs;

const extern SafeArray< SafeArray< SafeArray< SafeArray< Probability,
					 ENTROPY_NODES >,
				  PREV_COEF_CONTEXTS >,
			   COEF_BANDS >,
		    BLOCK_TYPES > k_default_coeff_probs;

#endif /* VP8_PROB_DATA_HH */
