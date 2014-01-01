#include <cassert>
#include <vector>

#include "frame.hh"
#include "exception.hh"

using namespace std;

template <class HeaderType, class MacroblockType>
Frame<HeaderType, MacroblockType>::Frame( const UncompressedChunk & chunk,
					  const unsigned int width,
					  const unsigned int height )
  : display_width_( width ),
    display_height_( height ),
    first_partition_( chunk.first_partition() ),
    dct_partitions_( chunk.dct_partitions( 1 << header_.log2_number_of_dct_partitions ) )
{}

template <class HeaderType, class MacroblockType>
void Frame<HeaderType, MacroblockType>::parse_macroblock_headers( const DecoderState & decoder_state )
{
  /* parse the macroblock headers */
  macroblock_headers_.initialize( macroblock_width_, macroblock_height_,
				  first_partition_, header_, decoder_state,
				  Y2_, Y_, U_, V_ );

  /* repoint Y2 above/left pointers to skip missing subblocks */
  relink_y2_blocks();
}

template <class HeaderType, class MacroblockType>
void Frame<HeaderType, MacroblockType>::parse_tokens( const DecoderState & decoder_state )
{
  macroblock_headers_.get().forall_ij( [&]( MacroblockType & macroblock,
					    const unsigned int column __attribute((unused)),
					    const unsigned int row )
				       { macroblock.parse_tokens( dct_partitions_.at( row % dct_partitions_.size() ),
								  decoder_state );
					 macroblock.dequantize( decoder_state ); } );
}

template <class HeaderType, class MacroblockType>
void Frame<HeaderType, MacroblockType>::decode( const DecoderState & decoder_state, Raster & raster ) const
{
  /* process each macroblock */
  macroblock_headers_.get().forall_ij( [&]( const MacroblockType & macroblock,
					    const unsigned int column,
					    const unsigned int row )
				       { macroblock.intra_predict_and_inverse_transform( raster.macroblock( column, row ) ); } );

  if ( header_.loop_filter_level ) {
    macroblock_headers_.get().forall_ij( [&]( const MacroblockType & macroblock,
					      const unsigned int column,
					      const unsigned int row )
					 { macroblock.loopfilter( decoder_state, raster.macroblock( column, row ) ); } );
  }
}

/* "above" for a Y2 block refers to the first macroblock above that actually has Y2 coded */
/* here we relink the "above" and "left" pointers after we learn the prediction mode
   for the block */
template <class HeaderType, class MacroblockType>
void Frame<HeaderType, MacroblockType>::relink_y2_blocks( void )
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

template class Frame< KeyFrameHeader, KeyFrameMacroblockHeader >;
template class Frame< InterFrameHeader, KeyFrameMacroblockHeader >;
