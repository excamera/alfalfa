/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef TOKEN_COSTS_HH
#define TOKEN_COSTS_HH

#include <array>

#include "safe_array.hh"
#include "decoder.hh"
#include "modemv_data.hh"
#include "tokens.hh"

const std::array<uint8_t, MAX_ENTROPY_TOKENS> prev_token_class =
  { 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0 };

class Costs
{
private:
  static uint32_t mv_component_cost( const int16_t num,
                                     const SafeArray<Probability, MV_PROB_CNT> & probs );

  template<unsigned int array_size, unsigned int prob_nodes, unsigned int token_count>
  void compute_cost( SafeArray<uint16_t, array_size> & costs_nodes,
                     const SafeArray<Probability, prob_nodes> & probabilities,
                     const SafeArray<TreeNode, token_count> & tree,
                     size_t tree_index = 0, uint16_t current_cost = 0 );

public:
  SafeArray<SafeArray<SafeArray<SafeArray<uint16_t,
                                          MAX_ENTROPY_TOKENS>,
                                PREV_COEF_CONTEXTS>,
                      COEF_BANDS>,
            BLOCK_TYPES> token_costs;

  SafeArray<SafeArray<uint16_t, num_y_modes + num_mv_refs>, 2> mbmode_costs;

  /* mv_component_costs[a][b][c]:
   * a is the axis, 0 for y and 1 for x,
   * b is the sign of the component, 0 for positive and 1 for negative,
   * c is the absolute value of the component.
   */
  SafeArray<SafeArray<SafeArray<uint32_t, 1024>, 2>, 2> mv_component_costs;
  SafeArray<SafeArray<SafeArray<uint32_t, 256>, 2>, 2> mv_sad_costs;


  SafeArray<SafeArray<SafeArray<uint16_t,
                                num_intra_b_modes>,
                      num_intra_b_modes>,
            num_intra_b_modes> bmode_costs;

  SafeArray<SafeArray<uint16_t, num_uv_modes>, 2> intra_uv_mode_costs;

  void fill_token_costs( const ProbabilityTables & probability_tables );

  void fill_mode_costs();
  void fill_mv_ref_costs( const ProbabilityArray< num_mv_refs > & mv_ref_probs );
  void fill_mv_component_costs( const SafeArray<SafeArray<Probability, MV_PROB_CNT>, 2> & motion_vector_probs );
  void fill_mv_sad_costs();

  uint32_t motion_vector_cost( const MotionVector & mv ) const;
  uint32_t sad_motion_vector_cost( const MotionVector & mv,
                                   const MotionVector & base,
                                   size_t weight ) const;

  template<class MacroblockType, class BlockType>
  uint32_t block_cost( const MacroblockType & frame_mb, const BlockType & block );

  static uint8_t token_for_coeff( int16_t coeff );
  static uint16_t coeff_base_cost( int16_t coeff );
};

#endif /* TOKEN_COSTS_HH */
