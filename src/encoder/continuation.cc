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
				    Raster::Block4 & raster )
{
  /* Get the residue */
  const auto residue = raster - prediction;

  for ( uint8_t i = 0; i < 16; i++ ) {
    block.mutable_coefficients().at( zigzag.at( i ) ) = residue.at( i / 4 ).at( i % 4 );
  }

  block.recalculate_has_nonzero();
}

template <>
void InterFrameMacroblock::rewrite_as_diff( Raster::Macroblock & raster,
					    const Raster::Macroblock & prediction )
{
  assert( inter_coded() );

  continuation_ = true;

  /* save old value so that we can run the loopfilter exactly as before */
  loopfilter_skip_subblock_edges_.initialize( Y2_.coded() and ( not has_nonzero_ ) );

  has_nonzero_ = false;

  /* rewrite the Y subblocks */
  Y_.forall_ij( [&] ( YBlock & block, unsigned int column, unsigned int row ) {
      rewrite_block_as_intra( block,
			      prediction.Y_sub.at( column, row ),
			      raster.Y_sub.at( column, row ) );

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

template <>
void InterFrame::rewrite_as_diff( const DecoderState & source_decoder_state,
				  const DecoderState & target_decoder_state,
				  const References & references,
				  const Raster & source_raster_preloop,
				  Raster & raster )
{
  /* match (normal) coefficient probabilities in frame header */
  for ( unsigned int i = 0; i < BLOCK_TYPES; i++ ) {
    for ( unsigned int j = 0; j < COEF_BANDS; j++ ) {
      for ( unsigned int k = 0; k < PREV_COEF_CONTEXTS; k++ ) {
	for ( unsigned int l = 0; l < ENTROPY_NODES; l++ ) {
	  const auto & source = source_decoder_state.probability_tables.coeff_probs.at( i ).at( j ).at( k ).at( l );
	  const auto & target = target_decoder_state.probability_tables.coeff_probs.at( i ).at( j ).at( k ).at( l );
	  header_.token_prob_update.at( i ).at( j ).at( k ).at( l ) = TokenProbUpdate( source != target, target );
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

  header_.intra_16x16_prob = decltype( header_.intra_16x16_prob )( update_y_mode_probs, new_y_mode_probs );

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

  header_.intra_chroma_prob = decltype( header_.intra_chroma_prob )( update_chroma_mode_probs, new_chroma_mode_probs );

  /* match motion_vector_probs in frame header */
  for ( uint8_t i = 0; i < 2; i++ ) {
    for ( uint8_t j = 0; j < MV_PROB_CNT; j++ ) {
      const auto & source = source_decoder_state.probability_tables.motion_vector_probs.at( i ).at( j );
      const auto & target = target_decoder_state.probability_tables.motion_vector_probs.at( i ).at( j );
      header_.mv_prob_update.at( i ).at( j ) = MVProbUpdate( source != target, target );      
    }
  }

  assert( not continuation_header_.initialized() );
  continuation_header_.initialize( true, true, true );

  const Quantizer frame_quantizer( header_.quant_indices );
  const auto segment_quantizers = calculate_segment_quantizers( target_decoder_state.segmentation );

  /* process each macroblock */
  macroblock_headers_.get().forall_ij( [&]( InterFrameMacroblock & macroblock,
					    const unsigned int column,
					    const unsigned int row ) {
					 const auto & quantizer = header_.update_segmentation.initialized()
					   ? segment_quantizers.at( macroblock.segment_id() )
					   : frame_quantizer;
					 if ( macroblock.inter_coded() ) {
					   macroblock.reconstruct_inter( quantizer,
									 references,
									 raster.macroblock( column, row ) );
					   macroblock.rewrite_as_diff( raster.macroblock( column, row ),
								       source_raster_preloop.macroblock( column, row ) );
					 } else {
					   macroblock.reconstruct_intra( quantizer,
									 raster.macroblock( column, row ) );
					 } } );

  // loopfilter( quantizer_filter_adjustments, raster );

  relink_y2_blocks();
}
