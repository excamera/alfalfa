#include <vector>

#include "macroblock_header.hh"

using namespace std;

intra_bmode subblock_mode( const intra_mbmode y_mode )
{
  switch ( y_mode ) {
  case DC_PRED: return B_DC_PRED;
  case V_PRED:  return B_VE_PRED;
  case H_PRED:  return B_HE_PRED;
  case TM_PRED: return B_TM_PRED;
  case B_PRED:  return B_DC_PRED;
  }
  assert( false );
  return intra_bmode();  
}

KeyFrameMacroblockHeader::KeyFrameMacroblockHeader( TwoD< KeyFrameMacroblockHeader >::Context & c,
						    BoolDecoder & data,
						    const KeyFrameHeader & key_frame_header,
						    const KeyFrameHeader::DerivedQuantities & probability_tables,
						    TwoD< Y2Block > & Y2,
						    TwoD< YBlock > & Y,
						    TwoD< UBlock > & U,
						    TwoD< VBlock > & V )
  : segment_id( key_frame_header.update_segmentation.initialized()
		and key_frame_header.update_segmentation.get().update_mb_segmentation_map,
		data, probability_tables.mb_segment_tree_probs ),
  mb_skip_coeff( key_frame_header.prob_skip_false.initialized()
		 ? Bool( data, key_frame_header.prob_skip_false.get() ) : Optional< Bool >() )
{
  /* Set Y prediction mode */
  Y2Block & y2_block = Y2.at( c.column, c.row );
  y2_block.prediction_mode() = data.tree< num_y_modes, intra_mbmode >( kf_y_mode_tree, kf_y_mode_probs );

  /* Set subblock prediction modes */
  YBlock default_block;
  default_block.prediction_mode() = B_DC_PRED;
  const auto overall_mode = subblock_mode( y2_block.prediction_mode() );

  for ( unsigned int row = c.row * 4; row < (c.row + 1) * 4; row++ ) {
    for ( unsigned int column = c.column * 4; column < (c.column + 1) * 4; column++ ) {
      YBlock block = Y.at( column, row );
      const auto above_mode = block.above().get_or( &default_block )->prediction_mode();
      const auto left_mode = block.left().get_or( &default_block )->prediction_mode();
      block.prediction_mode() = (y2_block.prediction_mode() == B_PRED)
	? data.tree< num_intra_b_modes, intra_bmode >( b_mode_tree, kf_b_mode_probs.at( above_mode ).at( left_mode ) )
	: overall_mode;
    }
  }

  /* Set U and V prediction modes */
  auto uv_mode = data.tree< num_uv_modes, intra_mbmode >( uv_mode_tree, kf_uv_mode_probs );
  for ( unsigned int row = c.row * 2; row < (c.row + 1) * 2; row++ ) {
    for ( unsigned int column = c.column * 2; column < (c.column + 1) * 2; column++ ) {
      U.at( column, row ).prediction_mode() = uv_mode;
      V.at( column, row ).prediction_mode() = uv_mode;
    }
  }
}
