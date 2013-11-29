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

  Y2Block & Y2;
  TwoDSubRange< YBlock > Y;
  TwoDSubRange< UBlock > U;
  TwoDSubRange< VBlock > V;

  KeyFrameMacroblockHeader( TwoD< KeyFrameMacroblockHeader >::Context & c,
			    BoolDecoder & data,
			    const KeyFrameHeader & key_frame_header,
			    const KeyFrameHeader::DerivedQuantities & probability_tables,
			    TwoD< Y2Block > & frame_Y2,
			    TwoD< YBlock > & frame_Y,
			    TwoD< UBlock > & frame_U,
			    TwoD< VBlock > & frame_V );
};

#endif /* MB_RECORDS_HH */
