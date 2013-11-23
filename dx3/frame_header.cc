#include "frame_header.hh"

using namespace std;

KeyFrameHeader::DerivedQuantities KeyFrameHeader::derived_quantities( void ) const
{
  ProbabilityArray< num_segments > mb_segment_tree_probs = {{ 255, 255, 255 }};

  for ( unsigned int i = 0; i < 3; i++ ) {
    if ( update_segmentation.initialized() and update_segmentation.get().update_mb_segmentation_map ) {
      mb_segment_tree_probs.at( i ) = update_segmentation.get().mb_segmentation_map.get().at( i ).get_or( 255 );
    }
  }

  DerivedQuantities ret;
  ret.mb_segment_tree_probs = mb_segment_tree_probs;

  return ret;
}
