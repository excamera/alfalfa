/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include "exception.hh"
#include "frame_pool.hh"

using namespace std;

/* helper to dequeue front element from a queue */
template <typename T>
static T dequeue( queue<T> & q )
{
  T ret { move( q.front() ) };
  q.pop();
  return ret;
}

template<class FrameType>
typename FramePool<FrameType>::FrameHolder FramePool<FrameType>::make_frame( const uint16_t width,
                                                                             const uint16_t height )
{
  unique_lock<mutex> lock { mutex_ };

  FrameHolder ret;

  if ( unused_frames_.empty() ) {
    ret.reset( new FrameType( width, height ) );
  } else {
    if ( (unused_frames_.front()->display_width() != width )
         or (unused_frames_.front()->display_height() != height ) ) {
      throw Unsupported( "frame size has changed" );
    } else {
      ret = dequeue( unused_frames_ );
    }
  }

  ret.get_deleter().set_frame_pool( this );

  return ret;
}

template<class FrameType>
void FramePool<FrameType>::free_frame( FrameType * frame )
{
  unique_lock<mutex> lock { mutex_ };

  assert( frame );
  unused_frames_.emplace( frame );
}

template<class FrameType>
void FrameDeleter<FrameType>::operator()( FrameType * frame ) const
{
  if ( frame_pool_ ) {
    frame_pool_->free_frame( frame );
  } else {
    delete frame;
  }
}

template<class FrameType>
FramePool<FrameType> * FrameDeleter<FrameType>::get_frame_pool() const
{
  return frame_pool_;
}

template<class FrameType>
void FrameDeleter<FrameType>::set_frame_pool( FramePool<FrameType> * pool )
{
  assert( not frame_pool_ );
  frame_pool_ = pool;
}

template<class FrameType>
static FramePool<FrameType> & global_frame_pool()
{
  static FramePool<FrameType> pool;
  return pool;
}

template<class FrameType>
FrameHandle<FrameType>::FrameHandle( const uint16_t width,
                                     const uint16_t height )
  : FrameHandle( width, height, global_frame_pool<FrameType>() )
{}

template<class FrameType>
FrameHandle<FrameType>::FrameHandle( const uint16_t width,
                                     const uint16_t height,
                                     FramePool<FrameType> & frame_pool )
  : frame_( frame_pool.make_frame( width, height ) )
{}

template class FrameHandle<KeyFrame>;
template class FrameHandle<InterFrame>;
