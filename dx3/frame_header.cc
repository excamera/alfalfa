#include "frame_header.hh"

using namespace std;

KeyFrameHeader::DerivedQuantities::DerivedQuantities( const KeyFrameHeader & header )
  : mb_segment_tree_probs( {{ 255, 255, 255 }} ),
    coeff_probs( k_default_coeff_probs ),
    quantizer( header.quant_indices ),
    segment_quantizers( {{ Quantizer( 0, header.quant_indices, header.update_segmentation ),
	    Quantizer( 1, header.quant_indices, header.update_segmentation ),
	    Quantizer( 2, header.quant_indices, header.update_segmentation ),
	    Quantizer( 3, header.quant_indices, header.update_segmentation ) }} )
{
  /* segmentation tree probabilities (if given in frame header) */
  if ( header.update_segmentation.initialized()
       and header.update_segmentation.get().mb_segmentation_map.initialized() ) {
    const auto & seg_map = header.update_segmentation.get().mb_segmentation_map.get();
    for ( unsigned int i = 0; i < mb_segment_tree_probs.size(); i++ ) {
      if ( seg_map.at( i ).initialized() ) {
	mb_segment_tree_probs.at( i ) = seg_map.at( i ).get();
      }
    }
  }

  /* token probabilities (if given in frame header) */
  for ( unsigned int i = 0; i < BLOCK_TYPES; i++ ) {
    for ( unsigned int j = 0; j < COEF_BANDS; j++ ) {
      for ( unsigned int k = 0; k < PREV_COEF_CONTEXTS; k++ ) {
	for ( unsigned int l = 0; l < ENTROPY_NODES; l++ ) {
	  const auto & node = header.token_prob_update.at( i ).at( j ).at( k ).at( l ).coeff_prob;
	  if ( node.initialized() ) {
	    coeff_probs.at( i ).at( j ).at( k ).at( l ) = node.get();
	  }
	}
      }
    }
  }
}
