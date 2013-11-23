#ifndef VP8_PROB_DATA_HH
#define VP8_PROB_DATA_HH

#include <array>

#include "bool_decoder.hh"

const unsigned int BLOCK_TYPES         = 4;
const unsigned int COEF_BANDS         = 8;
const unsigned int PREV_COEF_CONTEXTS = 3;
const unsigned int ENTROPY_NODES       = 11;

const unsigned int MV_PROB_CNT = 2 + 8 - 1 + 10;

const extern std::array< std::array< std::array< std::array< Probability,
							     ENTROPY_NODES >,
						 PREV_COEF_CONTEXTS >,
				     COEF_BANDS >,
			 BLOCK_TYPES > k_coeff_entropy_update_probs;

const extern std::array< std::array< std::array< std::array< Probability,
							     ENTROPY_NODES >,
						 PREV_COEF_CONTEXTS >,
				     COEF_BANDS >,
			 BLOCK_TYPES > k_default_coeff_probs;

#endif /* VP8_PROB_DATA_HH */
