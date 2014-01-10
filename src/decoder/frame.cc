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
void Frame<FrameHeaderType, MacroblockType>::parse_macroblock_headers( const DecoderState & decoder_state )
{
  fprintf( stderr, "New frame.\n" );

  /* parse the macroblock headers */
  macroblock_headers_.initialize( macroblock_width_, macroblock_height_,
				  first_partition_, header_, decoder_state,
				  Y2_, Y_, U_, V_ );

  /* repoint Y2 above/left pointers to skip missing subblocks */
  relink_y2_blocks();
}

template <class FrameHeaderType, class MacroblockType>
void Frame<FrameHeaderType, MacroblockType>::parse_tokens( const DecoderState & decoder_state )
{
  const Quantizer frame_quantizer( header_.quant_indices );

  const SafeArray< Quantizer, num_segments > segment_quantizers =
    {{ Quantizer( header_.quant_indices, decoder_state.segment_quantizer_adjustments.at( 0 ) ),
       Quantizer( header_.quant_indices, decoder_state.segment_quantizer_adjustments.at( 1 ) ),
       Quantizer( header_.quant_indices, decoder_state.segment_quantizer_adjustments.at( 2 ) ),
       Quantizer( header_.quant_indices, decoder_state.segment_quantizer_adjustments.at( 3 ) ) }};

  macroblock_headers_.get().forall_ij( [&]( MacroblockType & macroblock,
					    const unsigned int column __attribute((unused)),
					    const unsigned int row )
				       { macroblock.parse_tokens( dct_partitions_.at( row % dct_partitions_.size() ),
								  decoder_state );
					 macroblock.dequantize( frame_quantizer, segment_quantizers ); } );
}

template <class FrameHeaderType, class MacroblockType>
void Frame<FrameHeaderType, MacroblockType>::decode( const DecoderState & decoder_state, Raster & raster ) const
{
  const FilterParameters frame_loopfilter( header_.filter_type,
					   header_.loop_filter_level,
					   header_.sharpness_level );

  const SafeArray< FilterParameters, num_segments > segment_loopfilters =
    {{ FilterParameters( header_.filter_type, header_.loop_filter_level, header_.sharpness_level,
			 decoder_state.segment_filter_adjustments.at( 0 ) ),
       FilterParameters( header_.filter_type, header_.loop_filter_level, header_.sharpness_level,
			 decoder_state.segment_filter_adjustments.at( 1 ) ),
       FilterParameters( header_.filter_type, header_.loop_filter_level, header_.sharpness_level,
			 decoder_state.segment_filter_adjustments.at( 2 ) ),
       FilterParameters( header_.filter_type, header_.loop_filter_level, header_.sharpness_level,
			 decoder_state.segment_filter_adjustments.at( 3 ) ) }};

  /* process each macroblock */
  macroblock_headers_.get().forall_ij( [&]( const MacroblockType & macroblock,
					    const unsigned int column,
					    const unsigned int row )
				       { macroblock.predict_and_inverse_transform( raster.macroblock( column, row ) ); } );

  if ( header_.loop_filter_level ) {
    macroblock_headers_.get().forall_ij( [&]( const MacroblockType & macroblock,
					      const unsigned int column,
					      const unsigned int row )
					 { macroblock.loopfilter( decoder_state,
								  frame_loopfilter,
								  segment_loopfilters,
								  raster.macroblock( column, row ) ); } );
  }
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
      if ( block.prediction_mode() != B_PRED ) {
	above_coded.at( column ) = &block;
	left_coded.at( row ) = &block;
      }
    } );
}

template class Frame< KeyFrameHeader, KeyFrameMacroblock >;
template class Frame< InterFrameHeader, InterFrameMacroblock >;
