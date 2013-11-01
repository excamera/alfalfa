#ifndef MB_RECORDS_HH
#define MB_RECORDS_HH

#include <iostream>

#include "vp8_header_structures.hh"
#include "tree.cc"

typedef uint8_t SegmentID;

class KeyFrameMacroblockHeader
{
private:
  Optional< SegmentID > segment_id;

public:
  KeyFrameMacroblockHeader( BoolDecoder & data,
			    const KeyFrameHeader & key_frame_header )
    : segment_id( key_frame_header.update_segmentation.initialized()
		  ? key_frame_header.update_segmentation.get().update_mb_segmentation_map
		  ? Tree< 4, SegmentID >( { 2, 4, -0, -1, -2, -3 },
		    { key_frame_header.update_segmentation.get().mb_segmentation_map.get().at( 0 ).get_or( 255 ),
			key_frame_header.update_segmentation.get().mb_segmentation_map.get().at( 1 ).get_or( 255 ),
			key_frame_header.update_segmentation.get().mb_segmentation_map.get().at( 2 ).get_or( 255 )  }
		    ).get( data )
		  : Optional< SegmentID >()
		  : Optional< SegmentID >() )
  {}
};

#endif /* MB_RECORDS_HH */
