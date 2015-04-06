#include "raster_pool.hh"

RasterPool & RasterPool::global_pool( void )
{
  static RasterPool pool_;
  return pool_;
}

Raster * RasterPool::make_raster( const unsigned int display_width, const unsigned int display_height )
{
  if ( raster_pool_.empty() ) {
    return new Raster( display_width, display_height );
  }
  else {
    Raster * raster_ptr = raster_pool_.front().release();
    raster_pool_.pop();
    return raster_ptr;
  }
}

void RasterPool::free_raster( Raster * raster )
{
  raster_pool_.push( std::unique_ptr< Raster >( raster ) );
}
