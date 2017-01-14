/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef RASTER_POOL_HH
#define RASTER_POOL_HH

#include <mutex>

#include "vp8_raster.hh"

template<class RasterType> class RasterPool;
template<class RasterType> class VP8RasterHandle;

class RasterPoolDebug {
  public:
    // WARNING this will probably break everything!
    // you should only use it if you absolutely must
    // resize rasters while displaying and know exactly
    // what you're doing!
    static bool allow_resize;
};

class HashCachedRaster : public VP8Raster
{
private:
  mutable Optional<size_t> frozen_hash_ {};

  mutable std::mutex mutex_ {};

public:
  using VP8Raster::VP8Raster;

  size_t hash() const;
  void reset_cache();

  bool has_cache() const;
};

template<class RasterType>
class RasterDeleter
{
private:
  RasterPool<RasterType> * raster_pool_ = nullptr;

public:
  void operator()( RasterType * raster ) const;

  RasterPool<RasterType> * get_raster_pool( void ) const;
  void set_raster_pool( RasterPool<RasterType> * pool );
};

template<class RasterType>
class VP8MutableRasterHandle
{
friend class VP8RasterHandle<RasterType>;

private:
  std::unique_ptr<RasterType, RasterDeleter<RasterType>> raster_;

public:
  VP8MutableRasterHandle( const unsigned int display_width,
                          const unsigned int display_height );

  VP8MutableRasterHandle( const unsigned int display_width,
                          const unsigned int display_height,
                          RasterPool<RasterType> & raster_pool );

  operator const RasterType & () const { return *raster_; }
  operator RasterType & () { return *raster_; }

  const RasterType & get( void ) const { return *raster_; }
  RasterType & get( void ) { return *raster_; }
};

template<class RasterType>
class VP8RasterHandle
{
private:
  std::shared_ptr<const RasterType> raster_;

public:
  VP8RasterHandle( VP8MutableRasterHandle<RasterType> && mutable_raster );

  operator const RasterType & () const { return *raster_; }

  const RasterType & get( void ) const { return *raster_; }

  size_t hash( void ) const;

  bool operator==( const VP8RasterHandle<RasterType> & other ) const;
  bool operator!=( const VP8RasterHandle<RasterType> & other ) const;
};

using MutableRasterHandle = VP8MutableRasterHandle<HashCachedRaster>;
using RasterHandle = VP8RasterHandle<HashCachedRaster>;

using MutableSafeRasterHandle = VP8MutableRasterHandle<SafeRaster>;
using SafeRasterHandle = VP8RasterHandle<SafeRaster>;

#endif /* RASTER_POOL_HH */
