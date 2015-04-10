#include "frame_handle.hh"
#include "frame.hh"
#include <queue>

template <class FrameType>
class FramePool
{
private:
  std::queue<std::unique_ptr< FrameType >> frame_pool_ {};
public:
  std::unique_ptr< FrameType > make_frame( const bool show, const bool continuation,
					   const unsigned int width, const unsigned int height,
				       	   BoolDecoder & first_partition )
  {
    if ( frame_pool_.empty() ) {
      return std::unique_ptr< FrameType >( new FrameType( show, continuation, width, 
							  height, first_partition ) );
    }
    else {
      std::unique_ptr< FrameType > frame_ptr( std::move( frame_pool_.front() ) );
      frame_pool_.pop();
      frame_ptr->reinitialize( show, continuation, width, height, first_partition );
      return frame_ptr;
    }
  }

  void free_frame( std::unique_ptr< FrameType > & frame_ptr )
  {
    frame_pool_.push( std::move( frame_ptr ) );
  }
} ;

template <class FrameType>
static FramePool< FrameType > & global_frame_pool( void )
{
  static FramePool< FrameType > global_pool;
  return global_pool;
}

template <class FrameType>
FrameHandle< FrameType >::FrameHandle( const bool show, const bool continuation, 
				       const unsigned int width,
				       const unsigned int height, 
				       BoolDecoder & first_partition )
  : frame_ { global_frame_pool< FrameType >().make_frame( show, continuation, width,
							  height, first_partition ) }
{}

template <class FrameType>
FrameHandle< FrameType >::~FrameHandle()
{
  global_frame_pool< FrameType >().free_frame( frame_ );
}

template <class FrameType>
FrameHandle< FrameType >::FrameHandle( FrameHandle && other )
  : frame_ { std::move( other.frame_ ) }
{}

template <class FrameType>
FrameType * FrameHandle< FrameType >::operator->( void ) const
{
  return frame_.get();
}

template class FrameHandle< KeyFrame >;
template class FrameHandle< InterFrame >;
