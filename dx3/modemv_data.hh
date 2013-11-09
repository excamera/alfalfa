#ifndef MODEMV_DATA_HH
#define MODEMV_DATA_HH

#include <array>

enum intra_mbmode { DC_PRED, V_PRED, H_PRED, TM_PRED, B_PRED,
		    num_yv_modes = B_PRED, num_ymodes };
enum intra_bmode { B_DC_PRED, B_TM_PRED, B_VE_PRED, B_HE_PRED, B_LD_PRED,
		   B_RD_PRED, B_VR_PRED, B_VL_PRED, B_HD_PRED, B_HU_PRED,
		   num_intra_bmodes };



const extern std::array< uint8_t, 4 > kf_y_mode_probs;
const extern std::array< uint8_t, 3 > kf_uv_mode_probs;

const extern std::array< std::array< std::array< uint8_t, 9 >, 10 >, 10 > kf_b_mode_probs;

const extern std::array< int8_t, 8 > kf_y_mode_tree;

const extern std::array< int8_t, 8 > y_mode_tree;

const extern std::array< int8_t, 6 > uv_mode_tree;

const extern std::array< int8_t, 18 > b_mode_tree;

const extern std::array< int8_t, 14 > small_mv_tree;

const extern std::array< int8_t, 8 > mv_ref_tree;

const extern std::array< int8_t, 6 > submv_ref_tree;

const extern std::array< int8_t, 6 > split_mv_tree;

const extern std::array< uint8_t, 9 > default_b_mode_probs;

const extern std::array< std::array< uint8_t, 4 >, 6 > mv_counts_to_probs;

const extern std::array< uint8_t, 3 > split_mv_probs;

const extern std::array< std::array< uint8_t, 3 >, 5 > submv_ref_probs2;

const extern std::array< std::array< uint8_t, 16 >, 4 > mv_partitions;

const extern std::array< int8_t, 6 > segment_id_tree;

#endif /* MODEMV_DATA_HH */
