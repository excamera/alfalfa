#ifndef RASTER_POOL_HH
#define RASTER_POOL_HH

#include "raster.hh"

class RasterPool;
class RasterHandle;

class RasterDeleter
{
private:
  RasterPool * raster_pool_ = nullptr;

public:
  void operator()( Raster * raster ) const;

  RasterPool * get_raster_pool( void ) const;
  void set_raster_pool( RasterPool * pool );
};

typedef std::unique_ptr<Raster, RasterDeleter> RasterHolder;

class MutableRasterHandle
{
friend class RasterHandle;

private:
  RasterHolder raster_;

protected:
  RasterHolder & get_holder( void ); 

public:
  MutableRasterHandle( const unsigned int display_width, const unsigned int display_height );

  MutableRasterHandle( const unsigned int display_width, const unsigned int display_height, RasterPool & raster_pool );

  operator const Raster & () const { return *raster_; }
  operator Raster & () { return *raster_; }

  const Raster & get( void ) const { return *raster_; }
  Raster & get( void ) { return *raster_; }
};

class RasterHandle
{
private:
  std::shared_ptr<Raster> raster_;

public:
  RasterHandle( MutableRasterHandle && mutable_raster );

  operator const Raster & () const { return *raster_; }

  const Raster & get( void ) const { return *raster_; }

  size_t hash( void ) const;

  bool operator==( const RasterHandle & other ) const;
  bool operator!=( const RasterHandle & other ) const;
};


#endif /* RASTER_POOL_HH */
