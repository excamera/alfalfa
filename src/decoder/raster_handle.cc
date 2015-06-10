#include <memory>
#include <queue>
#include <functional>

#include "exception.hh"
#include "raster_handle.hh"

using namespace std;

class RasterDeleter
{
private:
  RasterPool * raster_pool_ = nullptr;

public:
  void operator()( Raster * raster ) const;
  void set_raster_pool( RasterPool * pool );
};

typedef unique_ptr<Raster, RasterDeleter> RasterHolder;

/* helper to dequeue front element from a queue */
template <typename T>
static T dequeue( queue<T> & q )
{
  T ret { move( q.front() ) };
  q.pop();
  return ret;
}

class RasterPool
{
private:
  queue<RasterHolder> unused_rasters_ {};

public:
  RasterHolder make_raster( const unsigned int display_width,
			    const unsigned int display_height )
  {
    RasterHolder ret;

    if ( unused_rasters_.empty() ) {
      ret.reset( new Raster( display_width, display_height ) );
    } else {
      if ( (unused_rasters_.front()->display_width() != display_width)
	   or (unused_rasters_.front()->display_height() != display_height) ) {
	throw Unsupported( "raster size has changed" );
      }

      ret = dequeue( unused_rasters_ );
    }

    ret.get_deleter().set_raster_pool( this );
    return ret;
  }

  void free_raster( Raster * raster )
  {
    assert( raster );
    unused_rasters_.emplace( raster );
  }
};

void RasterDeleter::operator()( Raster * raster ) const
{
  if ( raster_pool_ ) {
    raster_pool_->free_raster( raster );
  } else {
    delete raster;
  }
}

void RasterDeleter::set_raster_pool( RasterPool * pool )
{
  assert( not raster_pool_ );
  raster_pool_ = pool;
}

static RasterPool & global_raster_pool( void )
{
  static RasterPool pool;
  return pool;
}

RasterHandle::RasterHandle( const unsigned int display_width, const unsigned int display_height )
  : RasterHandle( display_width, display_height, global_raster_pool() )
{}

RasterHandle::RasterHandle( const unsigned int display_width, const unsigned int display_height, RasterPool & raster_pool )
  : raster_( raster_pool.make_raster( display_width, display_height ) )
{}
