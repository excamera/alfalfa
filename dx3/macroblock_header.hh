#ifndef MB_RECORDS_HH
#define MB_RECORDS_HH

#include "vp8_header_structures.hh"
#include "frame_header.hh"
#include "2d.hh"
#include "block.hh"

#include "tree.cc"

struct KeyFrameMacroblockHeader
{
  Optional< Tree< uint8_t, 4, segment_id_tree > > segment_id;
  Optional< Bool > mb_skip_coeff;

  KeyFrameMacroblockHeader( TwoD< KeyFrameMacroblockHeader >::Context & c,
			    BoolDecoder & data,
			    const KeyFrameHeader & key_frame_header,
			    const KeyFrameHeader::DerivedQuantities & probability_tables,
			    TwoD< Y2Block > & Y2,
			    TwoD< YBlock > & Y,
			    TwoD< UBlock > & U,
			    TwoD< VBlock > & V );
};

#endif /* MB_RECORDS_HH */
