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

#ifndef CAMERA_HH
#define CAMERA_HH

#include <linux/videodev2.h>

#include <unordered_map>

#include "optional.hh"
#include "file_descriptor.hh"
#include "mmap_region.hh"
#include "raster_handle.hh"
#include "frame_input.hh"
#include "jpeg.hh"

static const std::unordered_map<std::string, uint32_t> PIXEL_FORMAT_STRS {
  { "NV12", V4L2_PIX_FMT_NV12 },
  { "YUYV", V4L2_PIX_FMT_YUYV },
  { "YU12", V4L2_PIX_FMT_YUV420 },
  { "MJPG", V4L2_PIX_FMT_MJPEG }
};

class Camera : public FrameInput
{
private:
  const unsigned int NUM_BUFFERS = 4;

  uint16_t width_;
  uint16_t height_;

  FileDescriptor camera_fd_;
  std::vector<MMap_Region> kernel_v4l2_buffers_;
  unsigned int next_buffer_index = 0;

  uint32_t pixel_format_;

  Optional<JPEGDecompresser> jpegdec_ {};

public:
  Camera( const uint16_t width, const uint16_t height,
          const uint32_t pixel_format = V4L2_PIX_FMT_YUV420,
          const std::string device = "/dev/video0" );

  ~Camera();

  Optional<RasterHandle> get_next_frame() override;

  uint16_t display_width() { return width_; }
  uint16_t display_height() { return height_; }

  FileDescriptor & fd() { return camera_fd_; }
};

#endif
