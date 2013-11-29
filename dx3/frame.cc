#include "frame.hh"

KeyFrame::KeyFrame( UncompressedChunk & ,
		    const unsigned int width,
		    const unsigned int height )
  : macroblock_width( (width + 15) / 16 ),
    macroblock_height( (height + 15) / 16 )
{
}
