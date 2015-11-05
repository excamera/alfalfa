#include "frame.hh"
#include "macroblock.hh"

#include <algorithm>

using namespace std;

// FIXME, is this even right??
static unsigned calc_prob( unsigned false_count, unsigned total )
{
  if ( false_count == 0 ) {
    return 0;
  } else {
    return max( 1u, min( 255u, 256 * false_count / total ) );
  }
}

template <unsigned int size>
SafeArray< SafeArray< int16_t, size >, size > VP8Raster::Block<size>::operator-( const VP8Raster::Block<size> & other ) const
{
  SafeArray< SafeArray< int16_t, size >, size > ret;
  contents_.forall_ij( [&] ( const uint8_t & val, const unsigned int column, const unsigned int row ) {
      ret.at( row ).at( column ) = int16_t( val ) - int16_t( other.at( column, row ) );
    } );
  return ret;
}

template <BlockType initial_block_type, class PredictionMode>
static void write_block_as_intra( Block<initial_block_type, PredictionMode> & block,
                                  const ReferenceUpdater::Residue & residue )
{
  for ( uint8_t i = 0; i < 16; i++ ) {
    block.mutable_coefficients().at( zigzag.at( i ) ) = residue.at( i / 4 ).at( i % 4 );
  }

  block.calculate_has_nonzero();
}

template <BlockType initial_block_type, class PredictionMode>
void Block<initial_block_type, PredictionMode>::calculate_has_nonzero( void )
{
  has_nonzero_ = false;
  for ( uint8_t i = 0; i < 16; i++ ) {
    if ( coefficients_.at( i ) ) {
      has_nonzero_ = true;
      return;
    }
  }
}

template <BlockType initial_block_type, class PredictionMode>
void Block<initial_block_type, PredictionMode>::zero_out()
{
  has_nonzero_ = false;
  for ( uint8_t i = 0; i < 16; i++ ) {
    coefficients_.at( i ) = 0;
  }
}

template <>
void InterFrameMacroblock::zero_out()
{
  has_nonzero_ = false;

  Y2_.zero_out();

  Y_.forall( [&] ( YBlock & block ) { block.zero_out(); } );
  U_.forall( [&] ( UVBlock & block ) { block.zero_out(); } );
  V_.forall( [&] ( UVBlock & block ) { block.zero_out(); } );
}

template <>
RefUpdateFrameMacroblock::Macroblock( const typename TwoD< Macroblock >::Context & c,
                                      const ReferenceUpdater & ref_update,
	      			      TwoD< Y2Block > & frame_Y2,
	      			      TwoD< YBlock > & frame_Y,
	      			      TwoD< UVBlock > & frame_U,
	      			      TwoD< UVBlock > & frame_V )
  : context_( c ),
    segment_id_update_(),
    segment_id_( 0 ),
    mb_skip_coeff_(),
    header_(),
    Y2_( frame_Y2.at( c.column, c.row ) ),
    Y_( frame_Y, c.column * 4, c.row * 4 ),
    U_( frame_U, c.column * 2, c.row * 2 ),
    V_( frame_V, c.column * 2, c.row * 2 )
{
  if ( ref_update.macroblock( c.column, c.row ).initialized() ) {
    mb_skip_coeff_.initialize( false );
    return;
  }

  ReferenceUpdater::MacroblockDiff difference = ref_update.macroblock( c.column, c.row ).get();;

  /* Write the difference into the blocks */
  Y_.forall_ij( [&] ( YBlock & block, unsigned int column, unsigned int row ) {
      write_block_as_intra( block, difference.y_residue( column, row ) );
      has_nonzero_ |= block.has_nonzero();
   } );

  U_.forall_ij( [&] ( UVBlock & block, unsigned int column, unsigned int row ) {
      write_block_as_intra( block, difference.u_residue( column, row ) );

      has_nonzero_ |= block.has_nonzero();
    } );

  V_.forall_ij( [&] ( UVBlock & block, unsigned int column, unsigned int row ) {
      write_block_as_intra( block, difference.v_residue( column, row ) );

      has_nonzero_ |= block.has_nonzero();
    } );

  mb_skip_coeff_.initialize( not has_nonzero_ );
}

// Partial Reference Update Header
RefUpdateFrameHeader::RefUpdateFrameHeader( const reference_frame & frame, uint8_t skip_prob )
  : ref_to_update( frame - LAST_FRAME ), // Get a 0 index
    log2_number_of_dct_partitions( 0 ),
    token_prob_update(),
    prob_skip_false( true, skip_prob ),
    update_segmentation()
{}

StateUpdateFrameHeader::StateUpdateFrameHeader( const ProbabilityTables & source_probabilities,
                                                const ProbabilityTables & target_probabilities )
  : mv_prob_replacement(),
    log2_number_of_dct_partitions( 0 ),
    update_segmentation(),
    prob_skip_false()
{
  /* match motion_vector_probs in frame header */
  for ( uint8_t i = 0; i < 2; i++ ) {
    for ( uint8_t j = 0; j < MV_PROB_CNT; j++ ) {
      const auto & source = source_probabilities.motion_vector_probs.at( i ).at( j );
      const auto & target = target_probabilities.motion_vector_probs.at( i ).at( j );

      if ( target == 0 or (target != 1 and target % 2 != 0) ) {
        /* Only need to use a replacement if it can't be represented in the 7 bits of an MVProbUpdate */
        mv_prob_replacement.at( i ).at( j ) = MVProbReplacement( source != target, target );
      }
    }
  }
}

template <>
StateUpdateFrame::Frame( const ProbabilityTables & source_probabilities,
                         const ProbabilityTables & target_probabilities )
  : show_( false ),
    display_width_( 0 ),
    display_height_( 0 ),
    header_( source_probabilities, target_probabilities ),
    ref_updates_( false, false, false, false, false, false, false )
{}

template <>
void RefUpdateFrame::optimize_continuation_coefficients()
{
  TokenBranchCounts token_branch_counts;
  macroblock_headers_.get().forall( [&]( const RefUpdateFrameMacroblock & macroblock ) {
      macroblock.accumulate_token_branches( token_branch_counts ); } );

  for ( unsigned int i = 0; i < BLOCK_TYPES; i++ ) {
    for ( unsigned int j = 0; j < COEF_BANDS; j++ ) {
      for ( unsigned int k = 0; k < PREV_COEF_CONTEXTS; k++ ) {
        for ( unsigned int l = 0; l < ENTROPY_NODES; l++ ) {
          const unsigned int false_count = token_branch_counts.at( i ).at( j ).at( k ).at( l ).first;
          const unsigned int true_count = token_branch_counts.at( i ).at( j ).at( k ).at( l ).second;
          
          const unsigned int prob = calc_prob( false_count, false_count + true_count );

          assert( prob <= 255 );

          header_.token_prob_update.at( i ).at( j ).at( k ).at( l ) = TokenProbUpdate( true, prob );
        }
      }
    }
  }
}

// Partial Reference Update
template <>
RefUpdateFrame::Frame( const reference_frame & ref_to_update,
                       const ReferenceUpdater & update_info )
  : show_( false ),
    display_width_( update_info.width() ),
    display_height_( update_info.height() ),
    header_( ref_to_update, update_info.skip_probability() ),
    ref_updates_( ref_to_update == LAST_FRAME,
                  ref_to_update == GOLDEN_FRAME,
                  ref_to_update == ALTREF_FRAME,
                  false, false, false, false )

{
  macroblock_headers_.initialize( macroblock_width_, macroblock_height_, update_info,
                                  Y2_, Y_, U_, V_ );

  optimize_continuation_coefficients();
}

static RasterHandle make_new_reference( const reference_frame & frame, const RasterHandle & current,
                                        const RasterHandle & target, const ReferenceDependency & deps )
{
  assert( VP8Raster::macroblock_dimension( current.get().display_height() ) == deps.num_macroblocks_vert() );
  assert( VP8Raster::macroblock_dimension( current.get().display_width() ) == deps.num_macroblocks_horiz() );

  MutableRasterHandle mutable_raster( current.get().width(), current.get().height() );

  for ( unsigned row = 0; row < deps.num_macroblocks_vert(); row++ ) {
    for ( unsigned col = 0; col < deps.num_macroblocks_horiz(); col++ ) {
      if ( deps.need_update_macroblock( frame, col, row ) ) {
        mutable_raster.get().macroblock( col, row ) = target.get().macroblock( col, row );
      } else {
        mutable_raster.get().macroblock( col, row ) = current.get().macroblock( col, row );
      }
    }
  }

  return RasterHandle( move( mutable_raster ) );
}

ReferenceDependency::ReferenceDependency( const TwoD<InterFrameMacroblock> & frame_macroblocks )
  : width_( frame_macroblocks.width() ),
    height_( frame_macroblocks.height() ),
    needed_macroblocks_ { vector<vector<bool>>( height_, vector<bool>( width_, false ) ),
                          vector<vector<bool>>( height_, vector<bool>( width_, false ) ),
                          vector<vector<bool>>( height_, vector<bool>( width_, false ) ) }
{
  assert( width_ > 0 and height_ > 0 );
  frame_macroblocks.forall( [&]( const InterFrameMacroblock & macroblock ) {
                              for ( pair<unsigned, unsigned> coord : macroblock.required_macroblocks() ) {
                                reference_frame ref_dep = macroblock.header().reference();
                                needed_macroblocks_.at( ref_dep - 1 ).at( coord.second ).at( coord.first ) = true;
                              }
                            } );
}

bool ReferenceDependency::need_update_macroblock( const reference_frame & ref_frame, const unsigned col,
                                                  const unsigned row ) const
{
  return needed_macroblocks_.at( ref_frame - 1 ).at( row ).at( col );
}

ReferenceUpdater::ReferenceUpdater( const reference_frame & frame, const RasterHandle & current,
                                    const RasterHandle & target, const ReferenceDependency & deps )
  : new_reference_( make_new_reference( frame, current, target, deps ) ),
    diffs_( deps.num_macroblocks_vert(), vector<Optional<ReferenceUpdater::MacroblockDiff>>( deps.num_macroblocks_horiz() ) ),
    prob_skip_( 0 ), width_( target.get().width() ), height_( target.get().height() )
{
  assert( current.get().width() == target.get().width() and current.get().height() == target.get().height() );
  // FIXME revisit this function
  unsigned num_updated = 0;
  for ( unsigned row = 0; row < diffs_.size(); row++ ) {
    for ( unsigned col = 0; col < diffs_[ 0 ].size(); col++ ) {
      if ( deps.need_update_macroblock( frame, col, row ) ) {
        diffs_[ row ][ col ].initialize( ReferenceUpdater::MacroblockDiff( target.get().macroblock( col, row ), current.get().macroblock( col, row ) ) );
        num_updated++;
      }
    }
  }

  unsigned total = diffs_.size() * diffs_[ 0 ].size(); 

  prob_skip_ = calc_prob( total - num_updated, total );
}

uint8_t ReferenceUpdater::skip_probability() const
{
  return prob_skip_;
}

Optional<ReferenceUpdater::MacroblockDiff> ReferenceUpdater::macroblock( const unsigned int column,
                                                               const unsigned int row ) const
{
  return diffs_[ row ][ column ];
}

ReferenceUpdater::MacroblockDiff::MacroblockDiff()
  : Y_(), U_(), V_()
{}

ReferenceUpdater::MacroblockDiff::MacroblockDiff( const VP8Raster::Macroblock & lhs,
                                                  const VP8Raster::Macroblock & rhs )
  :Y_(), U_(), V_()
{
  for ( unsigned row = 0; row < Y_.size(); row++ ) {
    for ( unsigned col = 0; col < Y_.at( 0 ).size(); col++ ) {
      Y_.at( row ).at( col ) = lhs.Y_sub.at( col, row ) - rhs.Y_sub.at( col, row );
    }
  }

  for ( unsigned row = 0; row < U_.size(); row++ ) {
    for ( unsigned col = 0; col < U_.at( 0 ).size(); col++ ) {
      U_.at( row ).at( col ) = lhs.U_sub.at( col, row ) - rhs.U_sub.at( col, row );
    }
  }

  for ( unsigned row = 0; row < V_.size(); row++ ) {
    for ( unsigned col = 0; col < V_.at( 0 ).size(); col++ ) {
      V_.at( row ).at( col ) = lhs.V_sub.at( col, row ) - rhs.V_sub.at( col, row );
    }
  }
}

ReferenceUpdater::Residue ReferenceUpdater::MacroblockDiff::y_residue( const unsigned int column,
                                                                       const unsigned int row ) const
{
  return Y_.at( row ).at( column );
}

ReferenceUpdater::Residue ReferenceUpdater::MacroblockDiff::u_residue( const unsigned int column,
                                                                       const unsigned int row ) const
{
  return U_.at( row ).at( column );
}

ReferenceUpdater::Residue ReferenceUpdater::MacroblockDiff::v_residue( const unsigned int column,
                                                                       const unsigned int row ) const
{
  return V_.at( row ).at( column );
}

// Create a normal InterFrame which sets the state correctly for the target
// stream and removes redundant macroblocks that the RefUpdateFrames already
// took care of
template <>
InterFrame::Frame( const InterFrame & original,
                   const ProbabilityTables & source_probs,
                   const ProbabilityTables & target_probs,
                   const Optional<Segmentation> & target_segmentation,
                   const Optional<FilterAdjustments> & target_filter )
  : show_( original.show_frame() ),
    display_width_( original.display_width_ ),
    display_height_( original.display_height_ ),
    header_( original.header_ ),
    macroblock_headers_( true, macroblock_width_, macroblock_height_,
			 original.macroblock_headers_.get(),
			 Y2_, Y_, U_, V_ ),
    ref_updates_( original.ref_updates_ )
{
  Y2_.copy_from( original.Y2_ );
  Y_.copy_from( original.Y_ );
  U_.copy_from( original.U_ );
  V_.copy_from( original.V_ );

  /* match coefficient probabilities in frame header */
  for ( unsigned int i = 0; i < BLOCK_TYPES; i++ ) {
    for ( unsigned int j = 0; j < COEF_BANDS; j++ ) {
      for ( unsigned int k = 0; k < PREV_COEF_CONTEXTS; k++ ) {
	for ( unsigned int l = 0; l < ENTROPY_NODES; l++ ) {
	  const auto & source = source_probs.coeff_probs.at( i ).at( j ).at( k ).at( l );
	  const auto & target = target_probs.coeff_probs.at( i ).at( j ).at( k ).at( l );

	  header_.token_prob_update.at( i ).at( j ).at( k ).at( l ) = TokenProbUpdate( source != target, target );
	}
      }
    }
  }

  /* match segmentation if necessary */
  if ( target_segmentation.initialized() ) {
    assert( header_.update_segmentation.initialized() );  

    SegmentFeatureData segment_update;

    segment_update.segment_feature_mode = target_segmentation.get().absolute_segment_adjustments;

    for ( unsigned int i = 0; i < num_segments; i++ ) {
      /* these also default to 0 */
      const auto & q_adjustment = target_segmentation.get().segment_quantizer_adjustments.at( i );
      const auto & lf_adjustment = target_segmentation.get().segment_filter_adjustments.at( i );
      if ( q_adjustment ) {
        segment_update.quantizer_update.at( i ).initialize( q_adjustment );
      }

      if ( lf_adjustment ) {
        segment_update.loop_filter_update.at( i ).initialize( lf_adjustment );
      }
    }

    if ( header_.update_segmentation.get().segment_feature_data.initialized() ) {
      header_.update_segmentation.get().segment_feature_data.get() = segment_update;
    } else {
      header_.update_segmentation.get().segment_feature_data.initialize( segment_update );
    }

    /* FIXME this probably needs to be revisited */
    if ( not header_.update_segmentation.get().update_mb_segmentation_map ) {
      /* need to set up segmentation map */

      header_.update_segmentation.get().update_mb_segmentation_map = true;

      Array<Flagged<Unsigned<8>>, 3> mb_segmentation_map;

      /* don't optimize the tree probabilities for now */
      for ( unsigned int i = 0; i < 3; i++ ) {
	mb_segmentation_map.at( i ).initialize( 128 );
      }

      /* write segmentation updates into the macroblock headers */
      macroblock_headers_.get().forall( [&]( InterFrameMacroblock & macroblock ) {
	  macroblock.mutable_segment_id_update().initialize( macroblock.segment_id() ); } );

      if ( header_.update_segmentation.get().mb_segmentation_map.initialized() ) {
	header_.update_segmentation.get().mb_segmentation_map.get() = mb_segmentation_map;
      } else {
	header_.update_segmentation.get().mb_segmentation_map.initialize( mb_segmentation_map );
      }
    }
  }

  /* match FilterAdjustments if necessary */
  if ( target_filter.initialized() ) {
    assert( header_.mode_lf_adjustments.initialized() );

    ModeRefLFDeltaUpdate filter_update;

    /* these are 0 if not set */
    for ( unsigned int i = 0; i < target_filter.get().loopfilter_ref_adjustments.size(); i++ ) {
      const auto & value = target_filter.get().loopfilter_ref_adjustments.at( i );
      if ( value ) {
        filter_update.ref_update.at( i ).initialize( value );
      }
    }

    for ( unsigned int i = 0; i < target_filter.get().loopfilter_mode_adjustments.size(); i++ ) {
      const auto & value = target_filter.get().loopfilter_mode_adjustments.at( i );
      if ( value ) {
        filter_update.mode_update.at( i ).initialize( value );
      }
    }

    header_.mode_lf_adjustments.get() = Flagged< ModeRefLFDeltaUpdate >( true, filter_update );
  }

#if 0
  // FIXME the idea here is that if this frame is updating last, what is the point of using the partial reference
  // update to update last only to have this replace it? So the idea is that we do the work in the partial
  // reference update and then "zero out" the macroblocks here that have already been updated
  /* process each macroblock */
  macroblock_headers_.get().forall_ij( [&]( InterFrameMacroblock & macroblock,
					    const unsigned int column,
					    const unsigned int row ) {
                                         // FIXME if this is an inter coded macroblock that produces a result
                                         // which we've already put in the reference with a RefUpdateFrame
                                         // then zero it out
					 if ( macroblock.inter_coded() and
					   macroblock.zero_out() );
					 } } );
#endif
  relink_y2_blocks();
}
