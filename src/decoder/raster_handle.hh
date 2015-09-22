#ifndef RASTER_POOL_HH
#define RASTER_POOL_HH

#include "raster.hh"

class RasterPool;
class RasterHandle;

class HashCachedRaster : public Raster
{
private:
  mutable Optional<size_t> frozen_hash_ {};

public:
  using Raster::Raster;

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
  std::shared_ptr<HashCachedRaster> raster_;

public:
  RasterHandle( MutableRasterHandle && mutable_raster );

  operator const Raster & () const { return *raster_; }

  const Raster & get( void ) const { return *raster_; }

  size_t hash( void ) const;

  bool operator==( const RasterHandle & other ) const;
  bool operator!=( const RasterHandle & other ) const;
};

// Represents subtraction of two rasters for continuation
class RasterDiff
{
public:
  using Residue = SafeArray<SafeArray<int16_t, 4>, 4>;
  class MacroblockDiff {
  private:
    mutable SafeArray<SafeArray<Optional<Residue>, 4>, 4> Y_;
    mutable SafeArray<SafeArray<Optional<Residue>, 2>, 2> U_;
    mutable SafeArray<SafeArray<Optional<Residue>, 2>, 2> V_;

    const Raster::Macroblock & lhs_;
    const Raster::Macroblock & rhs_;
  public:
    MacroblockDiff( const Raster::Macroblock & lhs, const Raster::Macroblock & rhs );

    Residue y_residue( const unsigned int column, const unsigned int row ) const;
    Residue u_residue( const unsigned int column, const unsigned int row ) const;
    Residue v_residue( const unsigned int column, const unsigned int row ) const;
  };

private:
  RasterHandle lhs_;
  RasterHandle rhs_;

  mutable std::vector<std::vector<Optional<RasterDiff::MacroblockDiff>>> cache_;
public:
  RasterDiff( const RasterHandle & lhs, const RasterHandle & rhs );

  RasterDiff::MacroblockDiff macroblock( const unsigned int column, const unsigned int row ) const;
  
};


#endif /* RASTER_POOL_HH */
