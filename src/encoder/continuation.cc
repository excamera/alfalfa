#include "frame.hh"
#include "macroblock.hh"

using namespace std;

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
                                    const RasterDiff::Residue & residue )
{
  for ( uint8_t i = 0; i < 16; i++ ) {
    block.mutable_coefficients().at( zigzag.at( i ) ) = residue.at( i / 4 ).at( i % 4 );
  }

  block.recalculate_has_nonzero();
}

template <>
void InterFrameMacroblock::rewrite_as_diff( const RasterDiff::MacroblockDiff & difference )
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
      rewrite_block_as_intra( block, difference.y_residue( column, row ) );
      block.set_Y_without_Y2();
      has_nonzero_ |= block.has_nonzero();
    } );

  U_.forall_ij( [&] ( UVBlock & block, unsigned int column, unsigned int row ) {
      rewrite_block_as_intra( block, difference.u_residue( column, row ) );

      has_nonzero_ |= block.has_nonzero();
    } );

  V_.forall_ij( [&] ( UVBlock & block, unsigned int column, unsigned int row ) {
      rewrite_block_as_intra( block, difference.v_residue( column, row ) );

      has_nonzero_ |= block.has_nonzero();
    } );

  if ( mb_skip_coeff_.initialized() ) {
    mb_skip_coeff_.get() = not has_nonzero_;
  }
}

// Make Diff Frame
template <>
InterFrame::Frame( const InterFrame & original, const RasterDiff & continuation_diff, const bool shown,
                   const bool last_missing, const bool golden_missing, const bool alt_missing,
                   const ReplacementEntropyHeader & replacement_entropy_header,
                   const Optional<ModeRefLFDeltaUpdate> & filter_update,
                   const Optional<SegmentFeatureData> & segment_update )
  : show_ { shown },
    display_width_ { original.display_width_ },
    display_height_ { original.display_height_ },
    header_ { original.header_ },
    continuation_header_ { true, last_missing, golden_missing, alt_missing },
    macroblock_headers_ { true, macroblock_width_, macroblock_height_,
			  original.macroblock_headers_.get(),
			  Y2_, Y_, U_, V_ }
{
  Y2_.copy_from( original.Y2_ );
  Y_.copy_from( original.Y_ );
  U_.copy_from( original.U_ );
  V_.copy_from( original.V_ );

  header_.refresh_entropy_probs = false;
  continuation_header_.get().replacement_entropy_header.initialize( replacement_entropy_header ); 

  /* match FilterAdjustments if necessary */
  if ( filter_update.initialized() ) {
    assert( header_.mode_lf_adjustments.initialized() );

    header_.mode_lf_adjustments.get() = Flagged< ModeRefLFDeltaUpdate >( true, filter_update.get() );
  }

  /* match segmentation if necessary */
  if ( segment_update.initialized() ) {
    assert( header_.update_segmentation.initialized() );  

    if ( header_.update_segmentation.get().segment_feature_data.initialized() ) {
      header_.update_segmentation.get().segment_feature_data.get() = segment_update.get();
    } else {
      header_.update_segmentation.get().segment_feature_data.initialize( segment_update.get() );
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

  /* process each macroblock */
  macroblock_headers_.get().forall_ij( [&]( InterFrameMacroblock & macroblock,
					    const unsigned int column,
					    const unsigned int row ) {
					 if ( macroblock.inter_coded() and
					      continuation_header_.get().is_missing( macroblock.header().reference() ) ) {
					   macroblock.rewrite_as_diff( continuation_diff.macroblock( column, row ) );
					 } } );
  relink_y2_blocks();
}

// FIXME, probably not the right place for this...
RasterDiff::RasterDiff( const RasterHandle & lhs, const RasterHandle & rhs )
  : lhs_( lhs ), rhs_( rhs ),
    cache_( Raster::macroblock_dimension( lhs_.get().display_height() ), 
            vector<Optional<RasterDiff::MacroblockDiff>>( 
              Raster::macroblock_dimension( lhs_.get().display_width() ) ) )
            
{}

RasterDiff::MacroblockDiff RasterDiff::macroblock( const unsigned int column,
                                                   const unsigned int row ) const
{
  Optional<RasterDiff::MacroblockDiff> & macroblock = cache_[ row ][ column ];

  if ( not macroblock.initialized() ) {
    macroblock.initialize( lhs_.get().macroblock( column, row ), rhs_.get().macroblock( column, row ) );
  }

  return macroblock.get();
}

RasterDiff::MacroblockDiff::MacroblockDiff( const Raster::Macroblock & lhs,
                                            const Raster::Macroblock & rhs )
  :Y_(), U_(), V_(), lhs_( lhs ), rhs_( rhs )
{}

RasterDiff::Residue RasterDiff::MacroblockDiff::y_residue( const unsigned int column,
                                                           const unsigned int row ) const
{
  Optional<RasterDiff::Residue> & residue = Y_.at( row ).at( column );

  if ( not residue.initialized() ) {
    residue.initialize( lhs_.Y_sub.at( column, row ) - rhs_.Y_sub.at( column, row ) );
  }

  return residue.get();
}

RasterDiff::Residue RasterDiff::MacroblockDiff::u_residue( const unsigned int column,
                                                           const unsigned int row ) const
{
  Optional<RasterDiff::Residue> & residue = U_.at( row ).at( column );

  if ( not residue.initialized() ) {
    residue.initialize( lhs_.U_sub.at( column, row ) - rhs_.U_sub.at( column, row ) );
  }

  return residue.get();
}

RasterDiff::Residue RasterDiff::MacroblockDiff::v_residue( const unsigned int column,
                                                           const unsigned int row ) const
{
  Optional<RasterDiff::Residue> & residue = V_.at( row ).at( column );

  if ( not residue.initialized() ) {
    residue.initialize( lhs_.V_sub.at( column, row ) - rhs_.V_sub.at( column, row ) );
  }

  return residue.get();
}
