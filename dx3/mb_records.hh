#ifndef MB_RECORDS_HH
#define MB_RECORDS_HH

#include "vp8_header_structures.hh"
#include "frame_header.hh"

#include "tree.cc"

enum intra_mbmode { DC_PRED, V_PRED, H_PRED, TM_PRED, B_PRED };

class KeyFrameMacroblockHeader
{
private:
  Optional< uint8_t > segment_id;
  Optional< Bool > mb_skip_coeff;
  intra_mbmode y_mode;

public:
  KeyFrameMacroblockHeader( BoolDecoder & data,
			    const KeyFrameHeader & key_frame_header )
    : segment_id( key_frame_header.update_segmentation.initialized()
		  ? key_frame_header.update_segmentation.get().update_mb_segmentation_map
		  ? data.tree< 4, uint8_t >( { 2, 4, -0, -1, -2, -3 },
		    {   key_frame_header.update_segmentation.get().mb_segmentation_map.get().at( 0 ).get_or( 255 ),
			key_frame_header.update_segmentation.get().mb_segmentation_map.get().at( 1 ).get_or( 255 ),
			key_frame_header.update_segmentation.get().mb_segmentation_map.get().at( 2 ).get_or( 255 ) } )
		  : Optional< uint8_t >() : Optional< uint8_t >() ),
    mb_skip_coeff( key_frame_header.prob_skip_false.initialized()
		   ? Bool( data, key_frame_header.prob_skip_false.get() )
		   : Optional< Bool >() ),
    y_mode( data.tree< 5, intra_mbmode >( { -DC_PRED, 2, 4, 6, -V_PRED, -H_PRED, -TM_PRED, -B_PRED },
      { 145, 156, 163, 128 } ) )
  {}
};

#endif /* MB_RECORDS_HH */
