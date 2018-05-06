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
template class FrameDeleter<KeyFrame>;
template class FrameHandle<InterFrame>;
template class FrameDeleter<InterFrame>;
