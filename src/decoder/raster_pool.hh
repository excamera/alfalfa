#ifndef RASTER_POOL_HH
#define RASTER_POOL_HH

#include <memory>
#include <queue>
#include <functional>

#include "raster.hh"

class RasterPool
{
private:
  std::queue<std::unique_ptr< Raster >> raster_pool_ {};

public:
  RasterPool( void ) {}
  ~RasterPool( void ) {};
  Raster * make_raster( const unsigned int display_width, const unsigned int display_height );
  void free_raster( Raster * raster );
  static RasterPool & global_pool( void );
};

class RasterHandle
{
private:
  std::shared_ptr< Raster > raster_;

public:
  RasterHandle( const unsigned int display_width, const unsigned int display_height )
    : raster_( RasterPool::global_pool().make_raster( display_width, display_height ), 
		[]( Raster * raster ) 
		{
		  RasterPool::global_pool().free_raster( raster );
		} )
  {}

  RasterHandle( const std::shared_ptr< Raster > & other )
    : raster_( other )
  {}

  operator const Raster & () const { return *raster_; }
  operator Raster & () { return *raster_; }

  const Raster & get( void ) const { return *raster_; }
  Raster & get( void ) { return *raster_; }
};

#endif /* RASTER_POOL_HH */
