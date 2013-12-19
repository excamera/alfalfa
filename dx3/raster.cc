#include "raster.hh"

Raster::Macroblock::Block::Block( TwoD< Block >::Context & c,
				  TwoDSubRange< Component > & macroblock_component )
  : contents( macroblock_component, 4 * c.column, 4 * c.row, 4, 4 )
{}

Raster::Macroblock::Macroblock( TwoD< Macroblock >::Context & c, Raster & raster )
  : Y( raster.Y(), 16 * c.column, 16 * c.row, 16, 16 ),
    U( raster.U(), 8  * c.column, 8  * c.row, 8,  8 ),
    V( raster.V(), 8  * c.column, 8  * c.row, 8,  8 ),
    Y_blocks( 4, 4, Y ),
    U_blocks( 2, 2, U ),
    V_blocks( 2, 2, V )
{}

Raster::Raster( const unsigned int width, const unsigned int height,
		const unsigned int display_width, const unsigned int display_height )
  : width_( width ), height_( height ),
    display_width_( display_width ), display_height_( display_height ),
    macroblocks_( width_ / 16, height_ / 16, *this )
{}
