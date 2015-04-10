#include <memory>
#include <queue>
#include <functional>
#include "exception.hh"
#include "raster_handle.hh"

using RasterDeleter = std::function< void( Raster * ) >;

class RasterPool
{
private:
  std::queue<std::unique_ptr< Raster >> raster_pool_ {};

public:
  std::unique_ptr< Raster, RasterDeleter > make_raster( const unsigned int display_width, 
							const unsigned int display_height )
  {
    RasterDeleter deleter = [ this ]( Raster * raster )
			    {
  			      this->free_raster( raster );
  			    };

    if ( raster_pool_.empty() ) {
      return std::unique_ptr< Raster, RasterDeleter >( new Raster( display_width, display_height ), deleter );
    }
    else {
      if ( raster_pool_.front()->display_width() != display_width || raster_pool_.front()->display_height() != display_height ) {
	throw Unsupported( "rasters with different dimensions" );
      }
      std::unique_ptr< Raster, RasterDeleter > raster_ptr( raster_pool_.front().release(), deleter );
      raster_pool_.pop();
      return raster_ptr;
    }
  }
  void free_raster( Raster * raster )
  {
    raster_pool_.push( std::unique_ptr< Raster >( raster ) );
  }
};

static RasterPool & global_raster_pool( void )
{
  static RasterPool pool;
  return pool;
}

RasterHandle::RasterHandle( const unsigned int display_width, const unsigned int display_height )
  : raster_( std::move( global_raster_pool().make_raster( display_width, display_height ) ) )
{}
