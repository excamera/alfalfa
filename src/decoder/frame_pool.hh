/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

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
