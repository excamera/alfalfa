#include <cassert>
#include <vector>

#include "frame.hh"
#include "exception.hh"
#include "decoder.hh"

using namespace std;

template <class FrameHeaderType, class MacroblockType>
Frame<FrameHeaderType, MacroblockType>::Frame( const UncompressedChunk & chunk,
					       const unsigned int width,
					       const unsigned int height )
  : display_width_( width ),
    display_height_( height ),
    first_partition_( chunk.first_partition() ),
    dct_partitions_( chunk.dct_partitions( 1 << header_.log2_number_of_dct_partitions ) )
{}

template <class FrameHeaderType, class MacroblockType>
void Frame<FrameHeaderType, MacroblockType>::parse_macroblock_headers( SegmentationMap & segmentation_map,
								       const ProbabilityTables & probability_tables )
{
  /* parse the macroblock headers */
  macroblock_headers_.initialize( macroblock_width_, macroblock_height_,
				  first_partition_, header_,
				  segmentation_map,
				  probability_tables,
				  Y2_, Y_, U_, V_ );

  /* repoint Y2 above/left pointers to skip missing subblocks */
  relink_y2_blocks();
}

template <class FrameHeaderType, class MacroblockType>
void Frame<FrameHeaderType, MacroblockType>::parse_tokens( const QuantizerFilterAdjustments & quantizer_filter_adjustments,
							   const ProbabilityTables & probability_tables )
{
  const Quantizer frame_quantizer( header_.quant_indices );

  const SafeArray< Quantizer, num_segments > segment_quantizers =
    {{ Quantizer( header_.quant_indices, quantizer_filter_adjustments.segment_quantizer_adjustments.at( 0 ) ),
       Quantizer( header_.quant_indices, quantizer_filter_adjustments.segment_quantizer_adjustments.at( 1 ) ),
       Quantizer( header_.quant_indices, quantizer_filter_adjustments.segment_quantizer_adjustments.at( 2 ) ),
       Quantizer( header_.quant_indices, quantizer_filter_adjustments.segment_quantizer_adjustments.at( 3 ) ) }};

  macroblock_headers_.get().forall_ij( [&]( MacroblockType & macroblock,
					    const unsigned int column __attribute((unused)),
					    const unsigned int row )
				       { macroblock.parse_tokens( dct_partitions_.at( row % dct_partitions_.size() ),
								  probability_tables );
					 macroblock.dequantize( header_.update_segmentation.initialized()
								? segment_quantizers.at( macroblock.segment_id() )
								: frame_quantizer ); } );
}

template <class FrameHeaderType, class MacroblockType>
void Frame<FrameHeaderType, MacroblockType>::loopfilter( const QuantizerFilterAdjustments & quantizer_filter_adjustments, Raster & raster ) const
{
  if ( header_.loop_filter_level ) {
    const FilterParameters frame_loopfilter( header_.filter_type,
					     header_.loop_filter_level,
					     header_.sharpness_level );

    const SafeArray< FilterParameters, num_segments > segment_loopfilters =
      {{ FilterParameters( header_.filter_type, header_.loop_filter_level, header_.sharpness_level,
			   quantizer_filter_adjustments.segment_filter_adjustments.at( 0 ) ),
	 FilterParameters( header_.filter_type, header_.loop_filter_level, header_.sharpness_level,
			   quantizer_filter_adjustments.segment_filter_adjustments.at( 1 ) ),
	 FilterParameters( header_.filter_type, header_.loop_filter_level, header_.sharpness_level,
			   quantizer_filter_adjustments.segment_filter_adjustments.at( 2 ) ),
	 FilterParameters( header_.filter_type, header_.loop_filter_level, header_.sharpness_level,
			   quantizer_filter_adjustments.segment_filter_adjustments.at( 3 ) ) }};

    macroblock_headers_.get().forall_ij( [&]( const MacroblockType & macroblock,
					      const unsigned int column,
					      const unsigned int row )
					 { macroblock.loopfilter( quantizer_filter_adjustments,
								  header_.mode_lf_adjustments.initialized(),
								  header_.update_segmentation.initialized()
								  ? segment_loopfilters.at( macroblock.segment_id() )
								  : frame_loopfilter,
								  raster.macroblock( column, row ) ); } );
  }
}

template <>
void KeyFrame::decode( const QuantizerFilterAdjustments & quantizer_filter_adjustments,
		       Raster & raster ) const
{
  /* process each macroblock */
  macroblock_headers_.get().forall_ij( [&]( const KeyFrameMacroblock & macroblock,
					    const unsigned int column,
					    const unsigned int row )
				       { macroblock.intra_predict_and_inverse_transform( raster.macroblock( column, row ) ); } );

  loopfilter( quantizer_filter_adjustments, raster );
}

template <>
void InterFrame::decode( const QuantizerFilterAdjustments & quantizer_filter_adjustments,
			 const References & references,
			 Raster & raster ) const
{
  /* process each macroblock */
  macroblock_headers_.get().forall_ij( [&]( const InterFrameMacroblock & macroblock,
					    const unsigned int column,
					    const unsigned int row )
				       { if ( macroblock.inter_coded() ) {
					   macroblock.inter_predict_and_inverse_transform( references, raster.macroblock( column, row ) );
					 } else {
					   macroblock.intra_predict_and_inverse_transform( raster.macroblock( column, row ) );
					 } } );

  loopfilter( quantizer_filter_adjustments, raster );
}

/* "above" for a Y2 block refers to the first macroblock above that actually has Y2 coded */
/* here we relink the "above" and "left" pointers after we learn the prediction mode
   for the block */
template <class FrameHeaderType, class MacroblockType>
void Frame<FrameHeaderType, MacroblockType>::relink_y2_blocks( void )
{
  vector< Optional< const Y2Block * > > above_coded( macroblock_width_ );
  vector< Optional< const Y2Block * > > left_coded( macroblock_height_ );
  
  Y2_.forall_ij( [&]( Y2Block & block, const unsigned int column, const unsigned int row ) {
      block.set_above( above_coded.at( column ) );
      block.set_left( left_coded.at( row ) );
      if ( block.coded() ) {
	above_coded.at( column ) = &block;
	left_coded.at( row ) = &block;
      }
    } );
}

void Raster::copy( const Raster & other )
{
  assert( display_width_ == other.display_width_ );
  assert( display_height_ == other.display_height_ );

  Y_.copy( other.Y_ );
  U_.copy( other.U_ );
  V_.copy( other.V_ );
}

template <>
void KeyFrame::copy_to( const Raster & raster, References & references ) const
{
  references.last.copy( raster );
  references.golden.copy( raster );
  references.alternative_reference.copy( raster );
}

template <>
void InterFrame::copy_to( const Raster & raster, References & references ) const
{
  if ( header_.copy_buffer_to_alternate.initialized() ) {
    if ( header_.copy_buffer_to_alternate.get() == 1 ) {
      references.alternative_reference.copy( references.last );
    } else if ( header_.copy_buffer_to_alternate.get() == 2 ) {
      references.alternative_reference.copy( references.golden );
    }
  }

  if ( header_.copy_buffer_to_golden.initialized() ) {
    if ( header_.copy_buffer_to_golden.get() == 1 ) {
      references.golden.copy( references.last );
    } else if ( header_.copy_buffer_to_golden.get() == 2 ) {
      references.golden.copy( references.alternative_reference );
    }
  }

  if ( header_.refresh_golden_frame ) {
    references.golden.copy( raster );
  }

  if ( header_.refresh_alternate_frame ) {
    references.alternative_reference.copy( raster );
  }

  if ( header_.refresh_last ) {
    references.last.copy( raster );
  }
}

template class Frame< KeyFrameHeader, KeyFrameMacroblock >;
template class Frame< InterFrameHeader, InterFrameMacroblock >;
