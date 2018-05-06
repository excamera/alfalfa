/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Copyright 2013-2018 the Alfalfa authors
                       and the Massachusetts Institute of Technology

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

      1. Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include <memory>
#include <queue>
#include <functional>
#include <unordered_map>
#include <cassert>
#include <mutex>

#include "exception.hh"
#include "raster_handle.hh"

using namespace std;

/* helper to dequeue front element from a queue */
template <typename T>
static T dequeue( queue<T> & q )
{
  T ret { move( q.front() ) };
  q.pop();
  return ret;
}

bool RasterPoolDebug::allow_resize = false;

template<class RasterType>
class RasterPool
{
public:
  typedef std::unique_ptr<RasterType, RasterDeleter<RasterType>> VP8RasterHolder;

private:
  queue<VP8RasterHolder> unused_rasters_ {};

  mutex mutex_ {};

public:
  VP8RasterHolder make_raster( const unsigned int display_width,
                               const unsigned int display_height )
  {
    unique_lock<mutex> lock { mutex_ };

    VP8RasterHolder ret;

    if ( unused_rasters_.empty() ) {
      ret.reset( new RasterType( display_width, display_height ) );
    } else {
      if ( (unused_rasters_.front()->display_width() != display_width )
           or (unused_rasters_.front()->display_height() != display_height ) ) {
        if (RasterPoolDebug::allow_resize) {
          while (! unused_rasters_.empty()) {
            dequeue(unused_rasters_);
          }
          ret.reset(new RasterType(display_width, display_height));
        } else {
          throw Unsupported( "raster size has changed" );
        }
      } else {
        ret = dequeue( unused_rasters_ );
      }
    }

    ret.get_deleter().set_raster_pool( this );

    return ret;
  }

  void free_raster( RasterType * raster )
  {
    unique_lock<mutex> lock { mutex_ };

    assert( raster );
    unused_rasters_.emplace( raster );
  }
};

template<class RasterType>
void RasterDeleter<RasterType>::operator()( RasterType * raster ) const
{
  if ( raster_pool_ ) {
    raster_pool_->free_raster( raster );
  } else {
    delete raster;
  }
}

template<>
void RasterDeleter<HashCachedRaster>::operator()( HashCachedRaster * raster ) const
{
  if ( raster_pool_ ) {
    raster->reset_cache();
    raster_pool_->free_raster( raster );
  } else {
    delete raster;
  }
}

template<class RasterType>
RasterPool<RasterType> * RasterDeleter<RasterType>::get_raster_pool( void ) const
{
  return raster_pool_;
}

template<class RasterType>
void RasterDeleter<RasterType>::set_raster_pool( RasterPool<RasterType> * pool )
{
  assert( not raster_pool_ );
  raster_pool_ = pool;
}

template<class RasterType>
static RasterPool<RasterType> & global_raster_pool( void )
{
  static RasterPool<RasterType> pool;
  return pool;
}

template<class RasterType>
VP8MutableRasterHandle<RasterType>::VP8MutableRasterHandle( const unsigned int display_width,
                                                            const unsigned int display_height )
  : VP8MutableRasterHandle( display_width, display_height, global_raster_pool<RasterType>() )
{}

template<class RasterType>
VP8MutableRasterHandle<RasterType>::VP8MutableRasterHandle( const unsigned int display_width,
                                                            const unsigned int display_height,
                                                            RasterPool<RasterType> & raster_pool )
  : raster_( raster_pool.make_raster( display_width, display_height ) )
{}

template<>
VP8MutableRasterHandle<HashCachedRaster>::VP8MutableRasterHandle( const unsigned int display_width,
                                                                  const unsigned int display_height,
                                                                  RasterPool<HashCachedRaster> & raster_pool )
  : raster_( raster_pool.make_raster( display_width, display_height ) )
{
  assert( not raster_->has_cache() );
}

template<class RasterType>
VP8RasterHandle<RasterType>::VP8RasterHandle( VP8MutableRasterHandle<RasterType> && mutable_raster )
  : raster_( move( mutable_raster.raster_ ) )
{}

template<>
VP8RasterHandle<HashCachedRaster>::VP8RasterHandle( VP8MutableRasterHandle<HashCachedRaster> && mutable_raster )
  : raster_( move( mutable_raster.raster_ ) )
{
  assert( not raster_->has_cache() );
}

template<>
size_t VP8RasterHandle<HashCachedRaster>::hash( void ) const
{
  return raster_->hash();
}

template<>
bool VP8RasterHandle<HashCachedRaster>::operator==( const VP8RasterHandle<HashCachedRaster> & other ) const
{
  return hash() == other.hash();
}

template<>
bool VP8RasterHandle<HashCachedRaster>::operator!=( const VP8RasterHandle<HashCachedRaster> & other ) const
{
  return not operator==( other );
}

size_t HashCachedRaster::hash() const
{
  /* XXX the future had arrived */
  unique_lock<mutex> lock { mutex_ };

  if ( not frozen_hash_.initialized() ) {
    frozen_hash_.initialize( VP8Raster::raw_hash() );
  }

  return frozen_hash_.get();
}

void HashCachedRaster::reset_cache()
{
  frozen_hash_.clear();
}

bool HashCachedRaster::has_cache() const
{
  return frozen_hash_.initialized();
}

template class VP8MutableRasterHandle<HashCachedRaster>;
template class VP8RasterHandle<HashCachedRaster>;
template class RasterDeleter<HashCachedRaster>;
template class VP8MutableRasterHandle<SafeRaster>;
template class VP8RasterHandle<SafeRaster>;
template class RasterDeleter<SafeRaster>;
