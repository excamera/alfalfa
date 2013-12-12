#include "raster.hh"

Raster::Raster( const unsigned int width, const unsigned int height,
		const unsigned int display_width, const unsigned int display_height )
  : width_( width ), height_( height ),
    display_width_( display_width ), display_height_( display_height )
{}

Raster::Macroblock Raster::macroblock( const unsigned int macroblock_column,
				       const unsigned int macroblock_row )
{
  return { Y_.sub( macroblock_column * 16, macroblock_row * 16, 16, 16 ),
      U_.sub( macroblock_column * 8, macroblock_row * 8, 8, 8 ),
      V_.sub( macroblock_column * 8, macroblock_row * 8, 8, 8 ) };
}
