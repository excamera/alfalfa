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
  if ( not ref_update.macroblock( c.column, c.row ).initialized() ) {
    mb_skip_coeff_.initialize( true );
    return;
  }

  Y2_.set_coded( false );

  ReferenceUpdater::MacroblockDiff difference = ref_update.macroblock( c.column, c.row ).get();

  /* Write the difference into the blocks */
  Y_.forall_ij( [&] ( YBlock & block, unsigned int column, unsigned int row ) {
      block.set_Y_without_Y2();
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
  : token_prob_update(),
    intra_16x16_prob(),
    intra_chroma_prob(),
    mv_prob_replacement(),
    log2_number_of_dct_partitions( 0 ),
    update_segmentation(),
    prob_skip_false()
{
  /* match coefficient probabilities in frame header */
  for ( unsigned int i = 0; i < BLOCK_TYPES; i++ ) {
    for ( unsigned int j = 0; j < COEF_BANDS; j++ ) {
      for ( unsigned int k = 0; k < PREV_COEF_CONTEXTS; k++ ) {
	for ( unsigned int l = 0; l < ENTROPY_NODES; l++ ) {
	  const auto & source = source_probabilities.coeff_probs.at( i ).at( j ).at( k ).at( l );
	  const auto & target = target_probabilities.coeff_probs.at( i ).at( j ).at( k ).at( l );

	  token_prob_update.at( i ).at( j ).at( k ).at( l ) = TokenProbUpdate( source != target, target );
	}
      }
    }
  }

  /* match intra_16x16_probs in frame header */
  bool update_y_mode_probs = false;
  Array< Unsigned< 8 >, 4 > new_y_mode_probs;

  for ( unsigned int i = 0; i < 4; i++ ) {
    const auto & source = source_probabilities.y_mode_probs.at( i );
    const auto & target = target_probabilities.y_mode_probs.at( i );

    new_y_mode_probs.at( i ) = target;

    if ( source != target ) {
      update_y_mode_probs = true;
    }
  }

  if ( update_y_mode_probs ) {
    intra_16x16_prob.initialize( new_y_mode_probs );
  }

  /* match intra_chroma_prob in frame header */
  bool update_chroma_mode_probs = false;
  Array< Unsigned< 8 >, 3 > new_chroma_mode_probs;

  for ( unsigned int i = 0; i < 3; i++ ) {
    const auto & source = source_probabilities.uv_mode_probs.at( i );
    const auto & target = target_probabilities.uv_mode_probs.at( i );

    new_chroma_mode_probs.at( i ) = target;

    if ( source != target ) {
      update_chroma_mode_probs = true;
    }
  }

  if ( update_chroma_mode_probs ) {
    intra_chroma_prob.initialize( new_chroma_mode_probs );
  }

  /* match motion_vector_probs in frame header */
  for ( uint8_t i = 0; i < 2; i++ ) {
    for ( uint8_t j = 0; j < MV_PROB_CNT; j++ ) {
      const auto & source = source_probabilities.motion_vector_probs.at( i ).at( j );
      const auto & target = target_probabilities.motion_vector_probs.at( i ).at( j );

      mv_prob_replacement.at( i ).at( j ) = MVProbReplacement( source != target, target );
    }
  }
}

template <>
StateUpdateFrame::Frame( const ProbabilityTables & source_probabilities,
                         const ProbabilityTables & target_probabilities )
  : show_( false ),
    display_width_( 1 ), // Hack to make the underlying TwoD's have more than 0 width / height
    display_height_( 1 ),
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

static void translate_block_deps( vector<vector<bool>> & required, const int block_column, const int block_row, const MotionVector & mv,
                                  const unsigned size, const int width, const int height, const int macroblock_size )
{
  int row = block_row * size + ( mv.y() >> 3 );
  int col = block_column * size + ( mv.x() >> 3 );

  int upper_row = 0;
  int right_col = 0;
  int bottom_row = 0;
  int left_col = 0;
  
  if ( ( mv.x() & 7 ) == 0 and ( mv.y() & 7 ) == 0 ) {
    upper_row = row + size - 1;
    right_col = col + size - 1;
    bottom_row = row;
    left_col = col;
  }
  else {
    upper_row = row + size + 3;
    right_col = col + size + 3;
    bottom_row = row - 2;
    left_col = col - 2;
  }

  upper_row /= macroblock_size;
  right_col /= macroblock_size;
  bottom_row /= macroblock_size;
  left_col /= macroblock_size;

  for ( int cur_row = bottom_row; cur_row <= upper_row; cur_row++ ) {
    for ( int cur_col = left_col; cur_col <= right_col; cur_col++ ) {
      /* Make sure this isn't referencing something off the edge of the raster */
      int bounded_row = min( max( cur_row, 0 ), height - 1);
      int bounded_col = min( max( cur_col, 0 ), width - 1);

      required.at( bounded_row ).at( bounded_col ) = true;
    }
  }

#if 0
  // More explicit version, designed to loop over individual pixels
  for ( int cur_row = bottom_row; cur_row <= upper_row; cur_row++ ) {
    for ( int cur_col = left_col; cur_col <= right_col; cur_col++ ) {
      /* Make sure this isn't referencing something off the edge of the raster */
      int bounded_row = min( max( cur_row / macroblock_size, 0 ), height - 1);
      int bounded_col = min( max( cur_col / macroblock_size, 0 ), width - 1);

      required.at( bounded_row ).at( bounded_col ) = true;
    }
  }
#endif
}

template <>
void InterFrameMacroblock::record_dependencies( vector<vector<bool>> & required ) const
{
  /* FIXME Shares a lot of code with VP8Raster::Block::inter_predict and probably is fucking slow */
  if ( Y2_.prediction_mode() == SPLITMV ) {
    /* Each block has its own motion vector. Look at all of these and see which macroblocks they
     * point at */
    Y_.forall( [&] ( const YBlock & block ) { 
                 translate_block_deps( required, block.context().column, block.context().row,
                                       block.motion_vector(),
                                       4, context_.width,
                                       context_.height, 16 );
               } );
    U_.forall( [&] ( const UVBlock & block ) { 
                 translate_block_deps( required, block.context().column, block.context().row,
                                       block.motion_vector(),
                                       4, context_.width,
                                       context_.height, 8 );
               } );
  } else {
    translate_block_deps( required, Y_.at( 0, 0 ).context().column / 4, Y_.at( 0, 0 ).context().row / 4,
                         base_motion_vector(), 16, context_.width, context_.height, 16 );

    translate_block_deps( required, U_.at( 0, 0 ).context().column / 2, U_.at( 0, 0 ).context().row / 2,
                         U_.at( 0, 0 ).motion_vector(), 8, context_.width, context_.height, 8 );
  }
}

static RasterHandle make_new_reference( const reference_frame & frame, const VP8Raster & current,
                                        const VP8Raster & target, const ReferenceDependency & deps )
{
  assert( VP8Raster::macroblock_dimension( current.display_height() ) == deps.num_macroblocks_vert() );
  assert( VP8Raster::macroblock_dimension( current.display_width() ) == deps.num_macroblocks_horiz() );

  MutableRasterHandle mutable_raster( current.width(), current.height() );

  for ( unsigned row = 0; row < deps.num_macroblocks_vert(); row++ ) {
    for ( unsigned col = 0; col < deps.num_macroblocks_horiz(); col++ ) {
      if ( deps.need_update_macroblock( frame, col, row ) ) {
        mutable_raster.get().macroblock( col, row ) = target.macroblock( col, row );
      } else {
        mutable_raster.get().macroblock( col, row ) = current.macroblock( col, row );
      }
    }
  }

  return RasterHandle( move( mutable_raster ) );
}

static bool partially_equal_reference( const reference_frame & frame, const RasterHandle & reference,
                                       const RasterHandle & other_reference, const ReferenceDependency & deps )
{
  for ( unsigned row = 0; row < reference.get().macroblocks().height(); row++ ) {
    for ( unsigned col = 0; col < reference.get().macroblocks().width(); col++ ) {
      const VP8Raster::Macroblock & macroblock = reference.get().macroblock( col, row );
      const VP8Raster::Macroblock & other = other_reference.get().macroblock( col, row );
      if ( deps.need_update_macroblock( frame, col, row ) and macroblock != other ) {
        return false;
      }
    }
  }

  return true;
}

MissingTracker Decoder::find_missing( const References & refs,
                                      const ReferenceDependency & deps ) const
{
  return MissingTracker { not partially_equal_reference( LAST_FRAME, refs.last, references_.last, deps ),
                          not partially_equal_reference( GOLDEN_FRAME, refs.golden, references_.golden, deps ),
                          not partially_equal_reference( ALTREF_FRAME, refs.alternative_reference, references_.alternative_reference, deps ) };
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
                              if ( macroblock.inter_coded() ) {
                                reference_frame ref_dep = macroblock.header().reference();
                                macroblock.record_dependencies( needed_macroblocks_.at( ref_dep - 1 ) );
                              }
                            } );
}

bool ReferenceDependency::need_update_macroblock( const reference_frame & ref_frame, const unsigned col,
                                                  const unsigned row ) const
{
  return needed_macroblocks_.at( ref_frame - 1 ).at( row ).at( col );
}

static void check_ref( const EdgeExtendedRaster & ref, int column, int row )
{
  if (ref.at( column, row ) != 1 ) {
    cerr << "Unupdated pixel at " << column << " " << row << "\n";
    assert( false );
  }
}

template <unsigned size>
void VP8Raster::Block<size>::analyze_inter_predict( const MotionVector & mv, const TwoD< uint8_t > & ref )
{
  EdgeExtendedRaster reference( ref );
  const int source_column = context().column * size + (mv.x() >> 3);
  const int source_row = context().row * size + (mv.y() >> 3);

  if ( (mv.x() & 7) == 0 and (mv.y() & 7) == 0 ) {
    contents_.forall_ij( [&] ( uint8_t &, unsigned int column, unsigned int row )
			 { 
                           check_ref( reference, source_column + column, source_row + row );
                         } );
    return;
  }

  for ( uint8_t row = 0; row < size + 5; row++ ) {
    for ( uint8_t column = 0; column < size; column++ ) {
      const int real_row = source_row + row - 2;
      const int real_column = source_column + column;

      check_ref( reference, real_column - 2,   real_row );
      check_ref( reference, real_column - 1, real_row );
      check_ref( reference, real_column, real_row );
      check_ref( reference, real_column + 1, real_row );
      check_ref( reference, real_column + 2, real_row );
      check_ref( reference, real_column + 3, real_row );
    }
  }
}

template<>
void InterFrameMacroblock::analyze_dependencies( VP8Raster::Macroblock & raster, const VP8Raster & reference ) const
{
  if ( Y2_.prediction_mode() == SPLITMV ) {
    Y_.forall_ij( [&] ( const YBlock & block, const unsigned int column, const unsigned int row )
		  { raster.Y_sub.at( column, row ).analyze_inter_predict( block.motion_vector(), reference.Y() ); } );
    U_.forall_ij( [&] ( const UVBlock & block, const unsigned int column, const unsigned int row )
		  { raster.U_sub.at( column, row ).analyze_inter_predict( block.motion_vector(), reference.U() ); } );
  } else {
    raster.Y.analyze_inter_predict( base_motion_vector(), reference.Y() );
    raster.U.analyze_inter_predict( U_.at( 0, 0 ).motion_vector(), reference.U() );
  }
}

/**
 * Makes a new Raster with 1 stored in updated macroblocks and 0 in the other macroblocks
 */
static RasterHandle make_fake_reference( const reference_frame & frame, const ReferenceDependency & deps,
                                         const unsigned width, const unsigned height )
{
  MutableRasterHandle reference( width, height );
  for ( unsigned row = 0; row < deps.num_macroblocks_vert(); row++ ) {
    for ( unsigned col = 0; col < deps.num_macroblocks_horiz(); col++ ) {
      if ( deps.need_update_macroblock( frame, col, row ) ) {
        reference.get().macroblock(col, row).Y.mutable_contents().fill( 1 );
        reference.get().macroblock(col, row).U.mutable_contents().fill( 1 );
        reference.get().macroblock(col, row).V.mutable_contents().fill( 1 );
      }
      else {
        reference.get().macroblock(col, row).Y.mutable_contents().fill( 0 );
        reference.get().macroblock(col, row).U.mutable_contents().fill( 0 );
        reference.get().macroblock(col, row).V.mutable_contents().fill( 0 );
      }
    }
  }
  return move( reference );
}

template<>
void InterFrame::analyze_dependencies( const ReferenceDependency & deps ) const
{
  VP8Raster raster( display_width_, display_height_ );

  References fake_refs( display_width_, display_height_ );

  fake_refs.last = make_fake_reference( LAST_FRAME, deps, display_width_, display_height_ );
  fake_refs.golden = make_fake_reference( GOLDEN_FRAME, deps, display_width_, display_height_ );
  fake_refs.alternative_reference = make_fake_reference( ALTREF_FRAME, deps, display_width_, display_height_ );

  macroblock_headers_.get().forall_ij( [&] ( const InterFrameMacroblock & macroblock, const unsigned col, const unsigned row ) {
                                        if ( macroblock.inter_coded() ) {
                                          macroblock.analyze_dependencies( raster.macroblock( col, row ), fake_refs.at( macroblock.header().reference() ) );
                                        }
                                       } );
}

ReferenceUpdater::ReferenceUpdater( const reference_frame & frame, const VP8Raster & current,
                                    const VP8Raster & target, const ReferenceDependency & deps )
  : new_reference_( make_new_reference( frame, current, target, deps ) ),
    diffs_( deps.num_macroblocks_vert(), vector<Optional<ReferenceUpdater::MacroblockDiff>>( deps.num_macroblocks_horiz() ) ),
    prob_skip_( 0 ), width_( target.width() ), height_( target.height() )
{
  assert( current.width() == target.width() and current.height() == target.height() );
  // FIXME revisit this function
  unsigned num_updated = 0;
  for ( unsigned row = 0; row < diffs_.size(); row++ ) {
    for ( unsigned col = 0; col < diffs_[ 0 ].size(); col++ ) {
      if ( deps.need_update_macroblock( frame, col, row ) ) {
        diffs_[ row ][ col ].initialize( ReferenceUpdater::MacroblockDiff( target.macroblock( col, row ), current.macroblock( col, row ) ) );
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
