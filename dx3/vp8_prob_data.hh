#ifndef VP8_PROB_DATA_HH
#define VP8_PROB_DATA_HH

#include <array>

const unsigned int BLOCK_TYPES         = 4;
const unsigned int COEF_BANDS         = 8;
const unsigned int PREV_COEF_CONTEXTS = 3;
const unsigned int ENTROPY_NODES       = 11;

const unsigned int MV_PROB_CNT = 2 + 8 - 1 + 10;

const extern std::array< std::array< std::array< std::array< uint8_t, ENTROPY_NODES >, PREV_COEF_CONTEXTS >, COEF_BANDS >, BLOCK_TYPES > k_coeff_entropy_update_probs;

#endif /* VP8_PROB_DATA_HH */
