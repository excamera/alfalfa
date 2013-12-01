#include <cassert>
#include <vector>

#include "frame.hh"
#include "exception.hh"

using namespace std;

KeyFrame::KeyFrame( UncompressedChunk & chunk,
		    const unsigned int width,
		    const unsigned int height )
  : macroblock_width_( (width + 15) / 16 ),
    macroblock_height_( (height + 15) / 16 ),
    first_partition_( chunk.first_partition() ),
    dct_partitions_( chunk.dct_partitions( 1 << header_.log2_number_of_dct_partitions ) )
{}

void KeyFrame::calculate_probability_tables( void )
{
  /* calculate the probability tables from the defaults and any frame-header updates */
  probability_tables_ = header_.derived_quantities();
}

void KeyFrame::parse_macroblock_headers( void )
{
  /* parse the macroblock headers */
  macroblock_headers_.initialize( macroblock_width_, macroblock_height_,
				  first_partition_, header_, probability_tables_.get(),
				  Y2_, Y_, U_, V_ );
}

void KeyFrame::parse_tokens( void )
{
  /* repoint Y2 above/left pointers to skip missing subblocks */
  relink_y2_blocks();

  /* parse tokens */
  macroblock_headers_.get().forall( [&]( KeyFrameMacroblockHeader & macroblock,
					 const unsigned int column __attribute((unused)),
					 const unsigned int row )
				    {
				      macroblock.parse_tokens( dct_partitions_.at( row % dct_partitions_.size() ),
							       probability_tables_.get() );
				    } );
}

/* "above" for a Y2 block refers to the first macroblock above that actually has Y2 coded */
/* here we relink the "above" and "left" pointers after we learn the prediction mode
   for the block */
void KeyFrame::relink_y2_blocks( void )
{
  vector< Optional< Y2Block * > > above_coded( macroblock_width_, Optional< Y2Block * >() );
  vector< Optional< Y2Block * > > left_coded( macroblock_height_, Optional< Y2Block * >() );
  
  Y2_.forall( [&]( Y2Block & block, const unsigned int column, const unsigned int row ) {
      block.set_above( above_coded.at( column ) );
      block.set_left( left_coded.at( row ) );
      if ( block.prediction_mode() != B_PRED ) {
	above_coded.at( column ) = &block;
	left_coded.at( row ) = &block;
      }
    } );
}
