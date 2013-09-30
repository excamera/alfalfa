#include "frame_header.hh"

KeyFrameHeader::KeyFrameHeader( BoolDecoder & data )
  : color_space( data.get_bit() ),
    clamping_type( data.get_bit() ),
    update_segmentation( data )
{
}

UpdateSegmentation::UpdateSegmentation( BoolDecoder & data )
  : segmentation_enabled( data.get_bit() ),
    update_mb_segmentation_map( segmentation_enabled, [&](){ return data.get_bit(); } ),
    update_segment_feature_data( segmentation_enabled, [&](){ return data.get_bit(); } )
{
}
