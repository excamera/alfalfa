#ifndef RASTER_POOL_HH
#define RASTER_POOL_HH

#include "vp8_raster.hh"

class RasterPool;
class RasterHandle;

class HashCachedRaster : public VP8Raster
{
private:
  mutable Optional<size_t> frozen_hash_ {};

public:
  using VP8Raster::VP8Raster;

  size_t hash() const;
  void reset_cache();

  bool has_cache() const;
};

class RasterDeleter
{
private:
  RasterPool * raster_pool_ = nullptr;

public:
  void operator()( HashCachedRaster * raster ) const;

  RasterPool * get_raster_pool( void ) const;
  void set_raster_pool( RasterPool * pool );
};

typedef std::unique_ptr<HashCachedRaster, RasterDeleter> RasterHolder;

class MutableRasterHandle
{
friend class RasterHandle;

private:
  RasterHolder raster_;

public:
  MutableRasterHandle( const unsigned int display_width, const unsigned int display_height );

  MutableRasterHandle( const unsigned int display_width, const unsigned int display_height, RasterPool & raster_pool );

  operator const VP8Raster & () const { return *raster_; }
  operator VP8Raster & () { return *raster_; }

  const VP8Raster & get( void ) const { return *raster_; }
  VP8Raster & get( void ) { return *raster_; }
};

class RasterHandle
{
private:
  std::shared_ptr<const HashCachedRaster> raster_;

public:
  RasterHandle( MutableRasterHandle && mutable_raster );

  operator const VP8Raster & () const { return *raster_; }

  const VP8Raster & get( void ) const { return *raster_; }

  size_t hash( void ) const;

  bool operator==( const RasterHandle & other ) const;
  bool operator!=( const RasterHandle & other ) const;
};

#endif /* RASTER_POOL_HH */
