#include "frame_header.hh"

using namespace std;

KeyFrameHeader::mb_segment_tree_probs_type KeyFrameHeader::mb_segment_tree_probs( void ) const
{
  mb_segment_tree_probs_type ret;

  for ( unsigned int i = 0; i < 3; i++ ) {
    if ( update_segmentation.initialized() and update_segmentation.get().update_mb_segmentation_map ) {
      ret.at( i ) = update_segmentation.get().mb_segmentation_map.get().at( i ).get_or( 255 );
    } else {
      ret.at( i ) = 255;
    }
  }

  return ret;
}
