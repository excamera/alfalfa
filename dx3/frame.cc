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
    first_partition_( chunk.first_partition() )
{
  /* repoint Y2 above/left pointers */
  relink_y2_blocks();
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
