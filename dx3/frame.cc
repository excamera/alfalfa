#include <cassert>

#include "frame.hh"
#include "exception.hh"

KeyFrame::KeyFrame( UncompressedChunk & chunk,
		    const unsigned int width,
		    const unsigned int height )
  : macroblock_width( (width + 15) / 16 ),
    macroblock_height( (height + 15) / 16 ),
    first_partition( chunk.first_partition() )
{

}
