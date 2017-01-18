#ifndef CAMERA_HH
#define CAMERA_HH

#include <iostream>
#include <memory>
#include <cstdio>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#include "exception.hh"
#include "file_descriptor.hh"
#include "mmap_region.hh"
#include "raster_handle.hh"

using namespace std;

class Camera
{
private:
  uint16_t width_;
  uint16_t height_;

  FileDescriptor camera_fd_;
  std::unique_ptr<MMap_Region> mmap_region_;

  v4l2_buffer buffer_info_;
  int type_;

public:
  Camera( const uint16_t width, const uint16_t height )
    : width_( width ), height_( height ),
      camera_fd_( SystemCall( "open camera", open( "/dev/video0", O_RDWR ) ) ),
      mmap_region_(), buffer_info_(), type_()
  {
    v4l2_capability cap;
    SystemCall( "ioctl", ioctl( camera_fd_.fd_num(), VIDIOC_QUERYCAP, &cap ) );

    if ( not ( cap.capabilities & V4L2_CAP_VIDEO_CAPTURE ) ) {
      throw std::runtime_error( "this device does not handle video capture" );
    }

    /* setting the output format and size */
    v4l2_format format;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;

    SystemCall( "setting format", ioctl( camera_fd_.fd_num(), VIDIOC_S_FMT, &format ) );

    assert( format.fmt.pix.pixelformat == V4L2_PIX_FMT_NV12 );

    /* tell the v4l2 about our buffers */
    v4l2_requestbuffers buf_request;
    buf_request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf_request.memory = V4L2_MEMORY_MMAP;
    buf_request.count = 1;

    SystemCall( "buffer request", ioctl( camera_fd_.fd_num(), VIDIOC_REQBUFS, &buf_request ) );

    /* allocate buffers */
    memset( &buffer_info_, 0, sizeof( buffer_info_ ) );

    buffer_info_.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer_info_.memory = V4L2_MEMORY_MMAP;
    buffer_info_.index = 0;

    type_ = buffer_info_.type;
    SystemCall( "allocate buffer", ioctl( camera_fd_.fd_num(), VIDIOC_QUERYBUF, &buffer_info_ ) );

    /* mmap the thing */
    mmap_region_ = std::make_unique<MMap_Region>( buffer_info_.length, PROT_READ | PROT_WRITE,
                                                  MAP_SHARED, camera_fd_.fd_num() );

    SystemCall( "stream on", ioctl( camera_fd_.fd_num(), VIDIOC_STREAMON, &type_ ) );
  }

  ~Camera()
  {
    SystemCall( "stream off", ioctl( camera_fd_.fd_num(), VIDIOC_STREAMOFF, &type_ ) );
  }

  RasterHandle get_frame()
  {
    MutableRasterHandle raster_handle { width_, height_ };
    auto & raster = raster_handle.get();

    buffer_info_.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer_info_.memory = V4L2_MEMORY_MMAP;
    buffer_info_.index = 0;

    SystemCall( "queue", ioctl( camera_fd_.fd_num(), VIDIOC_QBUF, &buffer_info_ ) );

    memcpy( &raster.Y().at( 0, 0 ), mmap_region_->addr(), width_ * height_ );

    uint8_t * src_chroma_start = mmap_region_->addr() + width_ * height_;
    uint8_t * dst_cb_start = &raster.U().at( 0, 0 );
    uint8_t * dst_cr_start = &raster.V().at( 0, 0 );

    for ( size_t i = 0; i < width_ * height_ / 4; i++ ) {
      dst_cb_start[ i ] = src_chroma_start[ 2 * i ];
      dst_cr_start[ i ] = src_chroma_start[ 2 * i + 1 ];
    }

    SystemCall( "dequeue buffer", ioctl( camera_fd_.fd_num(), VIDIOC_DQBUF, &buffer_info_ ) );

    return RasterHandle{ std::move( raster_handle ) };
  }
};

#endif
