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

#ifndef FRAME_POOL_HH
#define FRAME_POOL_HH

#include <mutex>
#include <queue>
#include <memory>

#include "frame.hh"

template <class FrameType> class FramePool;

template<class FrameType>
class FrameDeleter
{
private:
  FramePool<FrameType> * frame_pool_ = nullptr;

public:
  void operator()( FrameType * frame ) const;

  FramePool<FrameType> * get_frame_pool( void ) const;
  void set_frame_pool( FramePool<FrameType> * pool );
};

template <class FrameType>
class FramePool
{
public:
  typedef std::unique_ptr<FrameType, FrameDeleter<FrameType>> FrameHolder;

private:
  std::queue<FrameHolder> unused_frames_ {};

  std::mutex mutex_ {};

public:
  FrameHolder make_frame( const uint16_t width,
                          const uint16_t height );

  void free_frame( FrameType * frame );
};

template<class FrameType>
class FrameHandle
{
private:
  std::unique_ptr<FrameType, FrameDeleter<FrameType>> frame_;

public:
  FrameHandle( const uint16_t width, const uint16_t height );
  FrameHandle( const uint16_t width, const uint16_t height,
	       FramePool<FrameType> & frame_pool );

  operator const FrameType & () const { return *frame_; }
  operator FrameType & () { return *frame_; }

  const FrameType & get( void ) const { return *frame_; }
  FrameType & get( void ) { return *frame_; }
};

using KeyFrameHandle = FrameHandle<KeyFrame>;
using InterFrameHandle = FrameHandle<InterFrame>;

#endif /* FRAME_POOL_HH */
