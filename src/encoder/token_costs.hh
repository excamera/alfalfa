#ifndef TOKEN_COSTS_HH
#define TOKEN_COSTS_HH

#include "safe_array.hh"
#include "decoder.hh"

class TokenCosts
{
private:
  void compute_cost( SafeArray<uint16_t, MAX_ENTROPY_TOKENS> & costs_nodes,
                     const SafeArray<Probability, ENTROPY_NODES> & probabilities,
                     size_t tree_index = 0, uint16_t current_cost = 0 );

  static uint8_t  inline complement( uint8_t prob );
  static uint16_t inline cost_zero( uint8_t prob );
  static uint16_t inline cost_one( uint8_t prob );
  static uint16_t inline cost_bit( uint8_t prob, bool b );


public:
  SafeArray<SafeArray<SafeArray<SafeArray<uint16_t,
                                          MAX_ENTROPY_TOKENS>,
                                PREV_COEF_CONTEXTS>,
                      COEF_BANDS>,
            BLOCK_TYPES> costs;

  void fill_costs( const ProbabilityTables & probability_tables );
  
  static uint16_t coeff_base_cost( int16_t coeff );
};

#endif /* TOKEN_COSTS_HH */
