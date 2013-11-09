#ifndef MB_RECORDS_HH
#define MB_RECORDS_HH

#include "vp8_header_structures.hh"
#include "frame_header.hh"
#include "modemv_data.hh"

#include "tree.cc"

class KeyFrameMacroblockHeader;

class IntraBMode : public Tree< intra_bmode, num_intra_bmodes, b_mode_tree >
{
private:
  static const std::array< uint8_t, num_intra_bmodes - 1 > &
    b_mode_probabilities( const unsigned int position,
			  const Array< IntraBMode, 16 > & prefix,
			  const KeyFrameMacroblockHeader & current,
			  const Optional< KeyFrameMacroblockHeader * > & above,
			  const Optional< KeyFrameMacroblockHeader * > & left );

public:
  IntraBMode( const unsigned int position,
	      const Array< IntraBMode, 16 > & prefix,
	      BoolDecoder & data,
	      const KeyFrameMacroblockHeader & current,
	      const Optional< KeyFrameMacroblockHeader * > & above,
	      const Optional< KeyFrameMacroblockHeader * > & left )
    : Tree( data, b_mode_probabilities( position, prefix, current, above, left ) )
  {}
};

struct KeyFrameMacroblockHeader
{
  Optional< Tree< uint8_t, 4, segment_id_tree > > segment_id;
  Optional< Bool > mb_skip_coeff;
  Tree< intra_mbmode, num_ymodes, kf_y_mode_tree > y_mode;

  Optional< Array< IntraBMode, 16 > > b_modes;

  KeyFrameMacroblockHeader( const Optional< KeyFrameMacroblockHeader * > & above,
			    const Optional< KeyFrameMacroblockHeader * > & left,
			    BoolDecoder & data,
			    const KeyFrameHeader & key_frame_header,
			    const KeyFrameHeader::DerivedQuantities & derived )
    : segment_id( key_frame_header.update_segmentation.initialized()
		  and key_frame_header.update_segmentation.get().update_mb_segmentation_map,
		  data, derived.mb_segment_tree_probs ),
      mb_skip_coeff( key_frame_header.prob_skip_false.initialized()
		     ? Bool( data, key_frame_header.prob_skip_false.get() )
		     : Optional< Bool >() ),
    y_mode( data, { 145, 156, 163, 128 } ),
    b_modes( y_mode == B_PRED, data, *this, above, left )
   {}
};

#endif /* MB_RECORDS_HH */
