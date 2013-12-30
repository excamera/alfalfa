#include <cassert>
#include <vector>

#include "frame.hh"
#include "exception.hh"

using namespace std;

KeyFrame::KeyFrame( const UncompressedChunk & chunk,
		    const unsigned int width,
		    const unsigned int height )
  : display_width_( width ),
    display_height_( height ),
    first_partition_( chunk.first_partition() ),
    dct_partitions_( chunk.dct_partitions( 1 << header_.log2_number_of_dct_partitions ) )
{
  assert( chunk.key_frame() );
}

void KeyFrame::parse_macroblock_headers( const DecoderState & decoder_state )
{
  /* parse the macroblock headers */
  macroblock_headers_.initialize( macroblock_width_, macroblock_height_,
				  first_partition_, header_, decoder_state,
				  Y2_, Y_, U_, V_ );
}

void KeyFrame::decode( const DecoderState & decoder_state )
{
  /* repoint Y2 above/left pointers to skip missing subblocks */
  relink_y2_blocks();

  /* process each macroblock */
  macroblock_headers_.get().forall_ij( [&]( KeyFrameMacroblockHeader & macroblock,
					    const unsigned int column __attribute((unused)),
					    const unsigned int row )
				       {
					 macroblock.parse_tokens( dct_partitions_.at( row % dct_partitions_.size() ),
								  decoder_state );
					 macroblock.dequantize( decoder_state );
					 macroblock.intra_predict_and_inverse_transform();
				       } );
}

void KeyFrame::loopfilter( const DecoderState & decoder_state ) {
  /* loop filter */
  if ( header_.loop_filter_level ) {
    macroblock_headers_.get().forall( [&]( KeyFrameMacroblockHeader & macroblock )
				      { macroblock.loopfilter( decoder_state ); } );
  }
}

/* "above" for a Y2 block refers to the first macroblock above that actually has Y2 coded */
/* here we relink the "above" and "left" pointers after we learn the prediction mode
   for the block */
void KeyFrame::relink_y2_blocks( void )
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

void KeyFrame::assign_output_raster( Raster & raster )
{
  macroblock_headers_.get().forall_ij( [&]( KeyFrameMacroblockHeader & macroblock,
					    const unsigned int column,
					    const unsigned int row )
				       { macroblock.assign_output_raster( raster.macroblock( column, row ) ); } );
}
