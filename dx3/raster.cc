#include "raster.hh"

template <unsigned int size>
Raster::Block<size>::Block( const typename TwoD< Block >::Context & c,
			    TwoD< Component > & raster_component )
  : contents( raster_component, size * c.column, size * c.row, size, size ),
    context( c )
{}

Raster::Macroblock::Macroblock( const TwoD< Macroblock >::Context & c, Raster & raster )
  : Y( raster.Y_bigblocks_.at( c.column, c.row ) ),
    U( raster.U_bigblocks_.at( c.column, c.row ) ),
    V( raster.V_bigblocks_.at( c.column, c.row ) ),
    Y_sub( raster.Y_subblocks_, 4 * c.column, 4 * c.row, 4, 4 ),
    U_sub( raster.U_subblocks_, 2 * c.column, 2 * c.row, 2, 2 ),
    V_sub( raster.V_subblocks_, 2 * c.column, 2 * c.row, 2, 2 )
{}

Raster::Raster( const unsigned int macroblock_width, const unsigned int macroblock_height,
		const unsigned int display_width, const unsigned int display_height )
  : width_( macroblock_width * 16 ), height_( macroblock_height * 16 ),
    display_width_( display_width ), display_height_( display_height )
{}
