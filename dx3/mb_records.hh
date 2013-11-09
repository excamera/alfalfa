#ifndef MB_RECORDS_HH
#define MB_RECORDS_HH

#include "vp8_header_structures.hh"
#include "frame_header.hh"

#include "tree.cc"

enum intra_mbmode { DC_PRED, V_PRED, H_PRED, TM_PRED, B_PRED,
		    num_yv_modes = B_PRED, num_ymodes };
enum intra_bmode { B_DC_PRED, B_TM_PRED, B_VE_PRED, B_HE_PRED, B_LD_PRED,
		   B_RD_PRED, B_VR_PRED, B_VL_PRED, B_HD_PRED, B_HU_PRED,
		   num_intra_bmodes };

extern std::array< int8_t, 6 > segment_id_tree;
extern std::array< int8_t, 8 > kf_y_mode_tree;

class KeyFrameMacroblockHeader
{
private:
  Optional< Tree< uint8_t, 4, segment_id_tree > > segment_id;
  Optional< Bool > mb_skip_coeff;
  Tree< intra_mbmode, num_ymodes, kf_y_mode_tree > y_mode;
  //  Optional< Enumeration< intra_bmode, 16 > > b_modes;

  static const std::array< uint8_t, num_intra_bmodes - 1 > &
  b_mode_probabilities( const unsigned int i,
			const Optional< KeyFrameMacroblockHeader * > & above,
			const Optional< KeyFrameMacroblockHeader * > & left );

public:
  KeyFrameMacroblockHeader( const Optional< KeyFrameMacroblockHeader * > & ,
			    const Optional< KeyFrameMacroblockHeader * > & ,
			    BoolDecoder & data,
			    const KeyFrameHeader & key_frame_header,
			    const KeyFrameHeader::DerivedQuantities & derived )
    : segment_id( key_frame_header.update_segmentation.initialized()
		  and key_frame_header.update_segmentation.get().update_mb_segmentation_map,
		  data, derived.mb_segment_tree_probs ),
      mb_skip_coeff( key_frame_header.prob_skip_false.initialized()
		     ? Bool( data, key_frame_header.prob_skip_false.get() )
		     : Optional< Bool >() ),
      y_mode( data, { 145, 156, 163, 128 } )
  {}
};

#endif /* MB_RECORDS_HH */
