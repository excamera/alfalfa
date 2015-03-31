#ifndef RASTER_POOL_HH
#define RASTER_POOL_HH

#include <memory>
#include <queue>

#include "raster.hh"

class RasterPool
{
private:
  std::queue<Raster> raster_pool_ {};

public:
  Raster & make_raster( void );
  void free_raster( Raster && raster );
};

class RasterHandle
{
private:
  std::shared_ptr< Raster > raster_;

public:
  RasterHandle( const unsigned int display_width, const unsigned int display_height )
    : raster_( std::make_shared<Raster>( display_width, display_height ) )
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
