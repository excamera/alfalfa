#include <vector>

#include "macroblock_header.hh"

using namespace std;

static intra_bmode implied_subblock_mode( const intra_mbmode y_mode )
{
  switch ( y_mode ) {
  case DC_PRED: return B_DC_PRED;
  case V_PRED:  return B_VE_PRED;
  case H_PRED:  return B_HE_PRED;
  case TM_PRED: return B_TM_PRED;
  case B_PRED:  break;
  }
  assert( false );
  return intra_bmode();  
}

KeyFrameMacroblockHeader::KeyFrameMacroblockHeader( TwoD< KeyFrameMacroblockHeader >::Context & c,
						    BoolDecoder & data,
						    const KeyFrameHeader & key_frame_header,
						    const KeyFrameHeader::DerivedQuantities & probability_tables,
						    TwoD< Y2Block > & frame_Y2,
						    TwoD< YBlock > & frame_Y,
						    TwoD< UVBlock > & frame_U,
						    TwoD< UVBlock > & frame_V )
  : segment_id_( key_frame_header.update_segmentation.initialized()
		 and key_frame_header.update_segmentation.get().update_mb_segmentation_map,
		 data, probability_tables.mb_segment_tree_probs ),
  mb_skip_coeff_( key_frame_header.prob_skip_false.initialized(),
		  data, key_frame_header.prob_skip_false.get() ),
  Y2_( frame_Y2.at( c.column, c.row ) ),
  Y_( frame_Y, c.column * 4, c.row * 4, 4, 4 ),
  U_( frame_U, c.column * 2, c.row * 2, 2, 2 ),
  V_( frame_V, c.column * 2, c.row * 2, 2, 2 )
{
  /* Set Y prediction mode */
  Y2_.set_prediction_mode( data.tree< num_y_modes, intra_mbmode >( kf_y_mode_tree, kf_y_mode_probs ) );
  Y2_.set_if_coded();

  /* Set subblock prediction modes */
  YBlock default_block;
  default_block.set_prediction_mode( B_DC_PRED );

  Y_.forall( [&]( YBlock & block )
	     {
	       if ( Y2_.prediction_mode() == B_PRED ) {
		 const auto above_mode = block.above().get_or( &default_block )->prediction_mode();
		 const auto left_mode = block.left().get_or( &default_block )->prediction_mode();
		 block.set_Y_without_Y2();
		 block.set_prediction_mode( data.tree< num_intra_b_modes,
					    intra_bmode >( b_mode_tree,
							   kf_b_mode_probs.at( above_mode ).at( left_mode ) ) );
	       } else {
		 block.set_prediction_mode( implied_subblock_mode( Y2_.prediction_mode() ) );
	       }
	     } );

  /* Set U and V prediction modes */
  auto uv_mode = data.tree< num_uv_modes, intra_mbmode >( uv_mode_tree, kf_uv_mode_probs );
  U_.forall( [&]( UVBlock & block ) { block.set_prediction_mode( uv_mode ); } );
  V_.forall( [&]( UVBlock & block ) { block.set_prediction_mode( uv_mode ); } );
}
