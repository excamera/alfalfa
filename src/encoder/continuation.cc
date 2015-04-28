#include "frame.hh"
#include "macroblock.hh"

template <unsigned int size>
SafeArray< SafeArray< int16_t, size >, size > Raster::Block<size>::operator-( const Raster::Block<size> & other ) const
{
  SafeArray< SafeArray< int16_t, size >, size > ret;
  contents_.forall_ij( [&] ( const uint8_t & val, const unsigned int column, const unsigned int row ) {
      ret.at( row ).at( column ) = int16_t( val ) - int16_t( other.at( column, row ) );
    } );
  return ret;
}

template <BlockType initial_block_type, class PredictionMode>
void Block<initial_block_type, PredictionMode>::recalculate_has_nonzero( void )
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
static void rewrite_block_as_intra( Block<initial_block_type, PredictionMode> & block,
				    const Raster::Block4 & prediction,
				    const Raster::Block4 & raster )
{
  /* Get the residue */
  const auto residue = raster - prediction;

  for ( uint8_t i = 0; i < 16; i++ ) {
    block.mutable_coefficients().at( zigzag.at( i ) ) = residue.at( i / 4 ).at( i % 4 );
  }

  block.recalculate_has_nonzero();
}

template <>
void InterFrameMacroblock::rewrite_as_diff( const Raster::Macroblock & raster,
					    const Raster::Macroblock & prediction )
{
  assert( inter_coded() );

  continuation_ = true;

  /* save old value so that we can run the loopfilter exactly as before */
  loopfilter_skip_subblock_edges_.initialize( Y2_.coded() and ( not has_nonzero_ ) );

  has_nonzero_ = false;

  /* erase the Y2 subblock contents */
  for ( uint8_t i = 0; i < 16; i++ ) {
    Y2_.mutable_coefficients().at( i ) = 0;
  }
  Y2_.recalculate_has_nonzero();

  assert( not Y2_.has_nonzero() );

  /* rewrite the Y subblocks */
  Y_.forall_ij( [&] ( YBlock & block, unsigned int column, unsigned int row ) {
      rewrite_block_as_intra( block,
			      prediction.Y_sub.at( column, row ),
			      raster.Y_sub.at( column, row ) );
      block.set_Y_without_Y2();
      has_nonzero_ |= block.has_nonzero();
    } );

  U_.forall_ij( [&] ( UVBlock & block, unsigned int column, unsigned int row ) {
      rewrite_block_as_intra( block,
			      prediction.U_sub.at( column, row ),
			      raster.U_sub.at( column, row ) );

      has_nonzero_ |= block.has_nonzero();
    } );

  V_.forall_ij( [&] ( UVBlock & block, unsigned int column, unsigned int row ) {
      rewrite_block_as_intra( block,
			      prediction.V_sub.at( column, row ),
			      raster.V_sub.at( column, row ) );

      has_nonzero_ |= block.has_nonzero();
    } );

  if ( mb_skip_coeff_.initialized() ) {
    mb_skip_coeff_.get() = not has_nonzero_;
  }
}

// Make Diff Frame
template <>
InterFrame::Frame( const InterFrame & original,
		   const DecoderState & source_decoder_state,
		   const DecoderState & target_decoder_state,
		   const Raster & source_raster,
		   const Raster & target_raster )
  : show_ { original.show_ },
    display_width_ { original.display_width_ },
    display_height_ { original.display_height_ },
    header_ { original.header_ },
    continuation_header_ { true, true, true, true},
    macroblock_headers_ { true, macroblock_width_, macroblock_height_,
			  original.macroblock_headers_.get(),
			  Y2_, Y_, U_, V_ }
{
  Y2_.copy_from( original.Y2_ );
  Y_.copy_from( original.Y_ );
  U_.copy_from( original.U_ );
  V_.copy_from( original.V_ );

  ReplacementEntropyHeader replacement_entropy_header;

  header_.refresh_entropy_probs = false;

  /* match (normal) coefficient probabilities in frame header */
  for ( unsigned int i = 0; i < BLOCK_TYPES; i++ ) {
    for ( unsigned int j = 0; j < COEF_BANDS; j++ ) {
      for ( unsigned int k = 0; k < PREV_COEF_CONTEXTS; k++ ) {
	for ( unsigned int l = 0; l < ENTROPY_NODES; l++ ) {
	  const auto & source = source_decoder_state.probability_tables.coeff_probs.at( i ).at( j ).at( k ).at( l );
	  const auto & target = target_decoder_state.probability_tables.coeff_probs.at( i ).at( j ).at( k ).at( l );

	  replacement_entropy_header.token_prob_update.at( i ).at( j ).at( k ).at( l ) = TokenProbUpdate( source != target, target );
	}
      }
    }
  }

  /* match intra_16x16_probs in frame header */
  bool update_y_mode_probs = false;
  Array< Unsigned< 8 >, 4 > new_y_mode_probs;

  for ( unsigned int i = 0; i < 4; i++ ) {
    const auto & source = source_decoder_state.probability_tables.y_mode_probs.at( i );
    const auto & target = target_decoder_state.probability_tables.y_mode_probs.at( i );

    new_y_mode_probs.at( i ) = target;

    if ( source != target ) {
      update_y_mode_probs = true;
    }
  }

  if ( update_y_mode_probs ) {
    replacement_entropy_header.intra_16x16_prob.initialize( new_y_mode_probs );
  }

  /* match intra_chroma_prob in frame header */
  bool update_chroma_mode_probs = false;
  Array< Unsigned< 8 >, 3 > new_chroma_mode_probs;

  for ( unsigned int i = 0; i < 3; i++ ) {
    const auto & source = source_decoder_state.probability_tables.uv_mode_probs.at( i );
    const auto & target = target_decoder_state.probability_tables.uv_mode_probs.at( i );

    new_chroma_mode_probs.at( i ) = target;

    if ( source != target ) {
      update_chroma_mode_probs = true;
    }
  }

  if ( update_chroma_mode_probs ) {
    replacement_entropy_header.intra_chroma_prob.initialize( new_chroma_mode_probs );
  }

  /* match motion_vector_probs in frame header */
  for ( uint8_t i = 0; i < 2; i++ ) {
    for ( uint8_t j = 0; j < MV_PROB_CNT; j++ ) {
      const auto & source = source_decoder_state.probability_tables.motion_vector_probs.at( i ).at( j );
      const auto & target = target_decoder_state.probability_tables.motion_vector_probs.at( i ).at( j );

      replacement_entropy_header.mv_prob_update.at( i ).at( j ) = MVProbReplacement( source != target, target );
    }
  }

  /* match FilterAdjustments if necessary */
  if ( target_decoder_state.filter_adjustments.initialized() ) {
    assert( header_.mode_lf_adjustments.initialized() );

    ModeRefLFDeltaUpdate filter_update;

    /* these are 0 if not set */
    for ( unsigned int i = 0; i < target_decoder_state.filter_adjustments.get().loopfilter_ref_adjustments.size(); i++ ) {
      const auto & value = target_decoder_state.filter_adjustments.get().loopfilter_ref_adjustments.at( i );
      if ( value ) {
	filter_update.ref_update.at( i ).initialize( value );
      }
    }

    for ( unsigned int i = 0; i < target_decoder_state.filter_adjustments.get().loopfilter_mode_adjustments.size(); i++ ) {
      const auto & value = target_decoder_state.filter_adjustments.get().loopfilter_mode_adjustments.at( i );
      if ( value ) {
	filter_update.mode_update.at( i ).initialize( value );
      }
    }

    header_.mode_lf_adjustments.get() = Flagged< ModeRefLFDeltaUpdate >( true, filter_update );
  }

  /* match segmentation if necessary */
  if ( target_decoder_state.segmentation.initialized() ) {
    assert( header_.update_segmentation.initialized() );  

    SegmentFeatureData segment_feature_data;

    segment_feature_data.segment_feature_mode = target_decoder_state.segmentation.get().absolute_segment_adjustments;

    for ( unsigned int i = 0; i < num_segments; i++ ) {
      /* these also default to 0 */
      const auto & q_adjustment = target_decoder_state.segmentation.get().segment_quantizer_adjustments.at( i );
      const auto & lf_adjustment = target_decoder_state.segmentation.get().segment_filter_adjustments.at( i );
      if ( q_adjustment ) {
	segment_feature_data.quantizer_update.at( i ).initialize( q_adjustment );
      }

      if ( lf_adjustment ) {
	segment_feature_data.loop_filter_update.at( i ).initialize( lf_adjustment );
      }
    }

    if ( header_.update_segmentation.get().segment_feature_data.initialized() ) {
      header_.update_segmentation.get().segment_feature_data.get() = segment_feature_data;
    } else {
      header_.update_segmentation.get().segment_feature_data.initialize( segment_feature_data );
    }

    if ( not header_.update_segmentation.get().update_mb_segmentation_map ) {
      /* need to set up segmentation map */

      header_.update_segmentation.get().update_mb_segmentation_map = true;

      Array< Flagged< Unsigned<8> >, 3 > mb_segmentation_map;

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

  continuation_header_.get().replacement_entropy_header.initialize( replacement_entropy_header );

  /* process each macroblock */
  macroblock_headers_.get().forall_ij( [&]( InterFrameMacroblock & macroblock,
					    const unsigned int column,
					    const unsigned int row ) {
					 if ( macroblock.inter_coded() ) {
					   macroblock.rewrite_as_diff( target_raster.macroblock( column, row ),
								       source_raster.macroblock( column, row ) );
					 } } );
  relink_y2_blocks();
}
