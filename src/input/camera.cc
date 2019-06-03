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

#include <iostream>
#include <memory>
#include <unordered_set>
#include <cstdio>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "camera.hh"
#include "exception.hh"
#include "jpeg.hh"

using namespace std;

unordered_set<uint32_t> SUPPORTED_FORMATS {
  { V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_MJPEG }
};

const int capture_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

Camera::Camera( const uint16_t width, const uint16_t height,
                const uint32_t pixel_format, const string device )
  : width_( width ), height_( height ),
    camera_fd_( SystemCall( "open camera", open( device.c_str(), O_RDWR ) ) ),
    kernel_v4l2_buffers_(), pixel_format_( pixel_format )
{
  v4l2_capability cap;
  SystemCall( "ioctl", ioctl( camera_fd_.fd_num(), VIDIOC_QUERYCAP, &cap ) );

  if ( not ( cap.capabilities & V4L2_CAP_VIDEO_CAPTURE ) ) {
    throw runtime_error( "this device does not handle video capture" );
  }

  if ( not SUPPORTED_FORMATS.count( pixel_format ) ) {
    throw runtime_error( "this pixel format is not implemented" );
  }

  /* setting the output format and size */
  v4l2_format format;
  format.type = capture_type;
  format.fmt.pix.pixelformat = pixel_format;
  format.fmt.pix.width = width;
  format.fmt.pix.height = height;

  SystemCall( "setting format", ioctl( camera_fd_.fd_num(), VIDIOC_S_FMT, &format ) );

  if ( format.fmt.pix.pixelformat != pixel_format or
       format.fmt.pix.width != width_ or
       format.fmt.pix.height != height_ ) {
    throw runtime_error( "couldn't configure the camera with the given format" );
  }

  /* tell the v4l2 about our buffers */
  v4l2_requestbuffers buf_request;
  buf_request.type = capture_type;
  buf_request.memory = V4L2_MEMORY_MMAP;
  buf_request.count = NUM_BUFFERS;

  SystemCall( "buffer request", ioctl( camera_fd_.fd_num(), VIDIOC_REQBUFS, &buf_request ) );

  if ( buf_request.count != NUM_BUFFERS ) {
    throw runtime_error( "couldn't get enough video4linux2 buffers" );
  }

  /* allocate buffers */
  for ( unsigned int i = 0; i < NUM_BUFFERS; i++ ) {
    v4l2_buffer buffer_info;
    buffer_info.type = capture_type;
    buffer_info.memory = V4L2_MEMORY_MMAP;
    buffer_info.index = i;

    SystemCall( "allocate buffer", ioctl( camera_fd_.fd_num(), VIDIOC_QUERYBUF, &buffer_info ) );

    kernel_v4l2_buffers_.emplace_back( buffer_info.length, PROT_READ | PROT_WRITE,
                                       MAP_SHARED, camera_fd_.fd_num(), buffer_info.m.offset );

    SystemCall( "enqueue buffer", ioctl( camera_fd_.fd_num(), VIDIOC_QBUF, &buffer_info ) );
  }

  SystemCall( "stream on", ioctl( camera_fd_.fd_num(), VIDIOC_STREAMON, &capture_type ) );
}

Camera::~Camera()
{
  SystemCall( "stream off", ioctl( camera_fd_.fd_num(), VIDIOC_STREAMOFF, &capture_type ) );
}

Optional<RasterHandle> Camera::get_next_frame()
{
  MutableRasterHandle raster_handle { width_, height_ };
  auto & raster = raster_handle.get();

  v4l2_buffer buffer_info;
  buffer_info.type = capture_type;
  buffer_info.memory = V4L2_MEMORY_MMAP;
  buffer_info.index = next_buffer_index;

  SystemCall( "dequeue buffer", ioctl( camera_fd_.fd_num(), VIDIOC_DQBUF, &buffer_info ) );

  const MMap_Region * const mmap_region_ = &kernel_v4l2_buffers_.at( next_buffer_index );

  switch( pixel_format_ ) {
  case V4L2_PIX_FMT_YUYV:
    {
    uint8_t * src = mmap_region_->addr();
    uint8_t * dst_y_start  = &raster.Y().at( 0, 0 );
    uint8_t * dst_cb_start = &raster.U().at( 0, 0 );
    uint8_t * dst_cr_start = &raster.V().at( 0, 0 );

    const size_t y_plane_lenght = width_ * height_;

    for ( size_t i = 0; i < y_plane_lenght; i++ ) {
      dst_y_start[ i ] = src[ i << 1 ];
    }

    size_t i = 0;

    for ( size_t row = 0; row < height_; row++ ) {
      if ( row % 2 == 1 ) { continue; }

      for ( size_t column = 0; column < width_ / 2; column++ ) {
        dst_cb_start[ i ] = src[ row * width_ * 2 + column * 4 + 1 ];
        dst_cr_start[ i ] = src[ row * width_ * 2 + column * 4 + 3 ];
        i++;
      }
    }
  }

  break;

  case V4L2_PIX_FMT_NV12:
    {
      memcpy( &raster.Y().at( 0, 0 ), mmap_region_->addr(), width_ * height_ );

      uint8_t * src_chroma_start = mmap_region_->addr() + width_ * height_;
      uint8_t * dst_cb_start = &raster.U().at( 0, 0 );
      uint8_t * dst_cr_start = &raster.V().at( 0, 0 );

      size_t chroma_length = width_ * height_ / 4;

      for ( size_t i = 0; i < chroma_length; i++ ) {
        dst_cb_start[ i ] = src_chroma_start[ 2 * i ];
        dst_cr_start[ i ] = src_chroma_start[ 2 * i + 1 ];
      }
    }

    break;

  case V4L2_PIX_FMT_YUV420:
    {
      memcpy( &raster.Y().at( 0, 0 ), mmap_region_->addr(), width_ * height_ );
      memcpy( &raster.U().at( 0, 0 ), mmap_region_->addr() + width_ * height_, width_ * height_ / 4 );
      memcpy( &raster.V().at( 0, 0 ), mmap_region_->addr() + width_ * height_ * 5 / 4, width_ * height_ / 4 );
    }

    break;

  case V4L2_PIX_FMT_MJPEG:
    {
      if ( jpegdec_.initialized() ) {
        jpegdec_.get().begin_decoding( { mmap_region_->addr(), buffer_info.bytesused } );
        if ( jpegdec_.get().width() != width_ or jpegdec_.get().height() != height_ ) {
          throw runtime_error( "size mismatch" );
        }
        jpegdec_.get().decode( raster );
      } else {
        jpegdec_.initialize();
        /* ignore first frame as can contain invalid JPEG data */
      }
    }
    break;
  }

  SystemCall( "enqueue buffer", ioctl( camera_fd_.fd_num(), VIDIOC_QBUF, &buffer_info ) );

  next_buffer_index = (next_buffer_index + 1) % NUM_BUFFERS;

  return RasterHandle{ move( raster_handle ) };
}
