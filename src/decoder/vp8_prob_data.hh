/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef VP8_PROB_DATA_HH
#define VP8_PROB_DATA_HH

#include "safe_array.hh"
#include "bool_decoder.hh"
#include "tokens.hh"
#include "modemv_data.hh"

constexpr unsigned int BLOCK_TYPES        = 4;
constexpr unsigned int COEF_BANDS         = 8;
constexpr unsigned int PREV_COEF_CONTEXTS = 3;

constexpr unsigned int MV_PROB_CNT = 2 + 8 - 1 + 10;

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

const extern ProbabilityArray< num_y_modes > k_default_y_mode_probs;

const extern ProbabilityArray< num_uv_modes > k_default_uv_mode_probs;

const extern SafeArray< SafeArray< Probability, MV_PROB_CNT >, 2 > k_mv_entropy_update_probs;

const extern SafeArray< SafeArray< Probability, MV_PROB_CNT >, 2 > k_default_mv_probs;

#endif /* VP8_PROB_DATA_HH */
