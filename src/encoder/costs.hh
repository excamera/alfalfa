/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef TOKEN_COSTS_HH
#define TOKEN_COSTS_HH

#include "safe_array.hh"
#include "decoder.hh"
#include "modemv_data.hh"

class Costs
{
private:
  template<unsigned int token_count, unsigned int prob_nodes>
  void compute_cost( SafeArray<uint16_t, token_count> & costs_nodes,
                     const SafeArray<Probability, prob_nodes> & probabilities,
                     const TreeArray<token_count> tree, size_t tree_index = 0,
                     uint16_t current_cost = 0 );
public:
  SafeArray<SafeArray<SafeArray<SafeArray<uint16_t,
                                          MAX_ENTROPY_TOKENS>,
                                PREV_COEF_CONTEXTS>,
                      COEF_BANDS>,
            BLOCK_TYPES> token_costs;

  SafeArray<SafeArray<uint16_t, num_y_modes>, 2> mbmode_costs;

  SafeArray<SafeArray<SafeArray<uint16_t,
                                num_intra_b_modes>,
                      num_intra_b_modes>,
            num_intra_b_modes> bmode_costs;

  SafeArray<SafeArray<uint16_t, num_uv_modes>, 2> intra_uv_mode_costs;

  void fill_token_costs( const ProbabilityTables & probability_tables );

  void fill_mode_costs();
  void fill_mode_costs( const ProbabilityArray< num_mv_refs > & mv_ref_probs );

  static uint16_t coeff_base_cost( int16_t coeff );
};

#endif /* TOKEN_COSTS_HH */
