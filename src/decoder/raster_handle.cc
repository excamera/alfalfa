#include <memory>
#include <queue>
#include <functional>

#include "exception.hh"
#include "raster_handle.hh"

using namespace std;

struct RasterDeleter
{
  void operator()( Raster * raster ) const;
};

typedef std::unique_ptr<Raster, RasterDeleter> RasterHolder;

/* helper to dequeue front element from a queue */
template <typename T>
T dequeue( queue<T> & q )
{
  T ret { move( q.front() ) };
  q.pop();
  return ret;
}

class RasterPool
{
private:
  std::queue<RasterHolder> raster_pool_ {};

public:
  RasterHolder make_raster( const unsigned int display_width,
			    const unsigned int display_height )
  {
    if ( raster_pool_.empty() ) {
      return RasterHolder( new Raster( display_width, display_height ) );
    }

    if ( (raster_pool_.front()->display_width() != display_width)
	 or (raster_pool_.front()->display_height() != display_height) ) {
      throw Unsupported( "raster size has changed" );
    }

    return dequeue( raster_pool_ );
  }

  void free_raster( Raster * raster )
  {
    raster_pool_.emplace( raster );
  }
};

static RasterPool & global_raster_pool( void )
{
  static RasterPool pool;
  return pool;
}

void RasterDeleter::operator()( Raster * raster ) const
{
  global_raster_pool().free_raster( raster );
}

RasterHandle::RasterHandle( const unsigned int display_width, const unsigned int display_height )
  : raster_( global_raster_pool().make_raster( display_width, display_height ) )
{}
