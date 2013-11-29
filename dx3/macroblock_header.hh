#ifndef MB_RECORDS_HH
#define MB_RECORDS_HH

#include "vp8_header_structures.hh"
#include "frame_header.hh"
#include "modemv_data.hh"
#include "2d.hh"
#include "block.hh"

#include "tree.cc"

struct KeyFrameMacroblockHeader;

class IntraBMode : public Tree< intra_bmode, num_intra_b_modes, b_mode_tree >
{
private:
  static const ProbabilityArray< num_intra_b_modes > &
    b_mode_probabilities( const unsigned int position,
			  const Array< IntraBMode, 16 > & prefix,
			  const KeyFrameMacroblockHeader & current,
			  const Optional< KeyFrameMacroblockHeader * > & above,
			  const Optional< KeyFrameMacroblockHeader * > & left );

public:
  IntraBMode( BoolDecoder & data,
	      const unsigned int position,
	      const EnumerateContext< IntraBMode, 16 > & prefix,
	      const KeyFrameMacroblockHeader & current,
	      const Optional< KeyFrameMacroblockHeader * > & above,
	      const Optional< KeyFrameMacroblockHeader * > & left )
    : Tree( data, b_mode_probabilities( position, prefix, current, above, left ) )
  {}

  using Tree::Tree;
};

struct KeyFrameMacroblockHeader
{
  Optional< Tree< uint8_t, 4, segment_id_tree > > segment_id;
  Optional< Bool > mb_skip_coeff;
  Tree< intra_mbmode, num_y_modes, kf_y_mode_tree > y_mode;
  Optional< EnumerateContext< IntraBMode, 16 > > b_modes;
  Tree< intra_mbmode, num_uv_modes, uv_mode_tree > uv_mode;

  KeyFrameMacroblockHeader( TwoD< KeyFrameMacroblockHeader >::Context & c,
			    BoolDecoder & data,
			    const KeyFrameHeader & key_frame_header,
			    const KeyFrameHeader::DerivedQuantities & probability_tables,
			    TwoD< Block< intra_mbmode > > & ,
			    TwoD< Block< intra_bmode > > & ,
			    TwoD< Block< intra_mbmode > > & ,
			    TwoD< Block< intra_mbmode > > &  )
    : segment_id( key_frame_header.update_segmentation.initialized()
		  and key_frame_header.update_segmentation.get().update_mb_segmentation_map,
		  data, probability_tables.mb_segment_tree_probs ),
      mb_skip_coeff( key_frame_header.prob_skip_false.initialized()
		     ? Bool( data, key_frame_header.prob_skip_false.get() ) : Optional< Bool >() ),
    y_mode( data, kf_y_mode_probs ),
    b_modes( y_mode == B_PRED, data, *this, c.above, c.left ),
    uv_mode( data, kf_uv_mode_probs )
  {}
};

#endif /* MB_RECORDS_HH */
