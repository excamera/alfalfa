#include "raster.hh"

Raster::Block::Block( TwoD< Block >::Context & c,
		      TwoD< Component > & raster_component )
  : contents( raster_component, 4 * c.column, 4 * c.row, 4, 4 )
{}

Raster::Macroblock::Macroblock( TwoD< Macroblock >::Context & c, Raster & raster )
  : Y( raster.Y(), 16 * c.column, 16 * c.row, 16, 16 ),
    U( raster.U(), 8  * c.column, 8  * c.row, 8,  8 ),
    V( raster.V(), 8  * c.column, 8  * c.row, 8,  8 ),
    Y_blocks( raster.Y_blocks(), 4 * c.column, 4 * c.row, 4, 4 ),
    U_blocks( raster.U_blocks(), 2 * c.column, 2 * c.row, 2, 2 ),
    V_blocks( raster.V_blocks(), 2 * c.column, 2 * c.row, 2, 2 )
{}

Raster::Raster( const unsigned int macroblock_width, const unsigned int macroblock_height,
		const unsigned int display_width, const unsigned int display_height )
  : width_( macroblock_width * 16 ), height_( macroblock_height * 16 ),
    display_width_( display_width ), display_height_( display_height )
{}
