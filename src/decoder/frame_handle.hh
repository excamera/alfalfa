#ifndef FRAME_HANDLE_HH
#define FRAME_HANDLE_HH

#include "bool_decoder.hh"
#include <memory>

template <class FrameType>
class FrameHandle
{
private:
  std::unique_ptr< FrameType > frame_;
public:
  FrameHandle( const bool show, const bool continuation, const unsigned int width,
	       const unsigned int height, BoolDecoder & first_partition );
  ~FrameHandle();

  FrameHandle( const FrameHandle & ) = delete;
  FrameHandle & operator=(const FrameHandle & ) = delete;

  FrameHandle( FrameHandle && other );

  FrameType * operator->( void ) const;
} ;

#endif
