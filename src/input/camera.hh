/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef CAMERA_HH
#define CAMERA_HH

#include <linux/videodev2.h>

#include <unordered_map>

#include "optional.hh"
#include "file_descriptor.hh"
#include "mmap_region.hh"
#include "raster_handle.hh"
#include "frame_input.hh"

static const std::unordered_map<std::string, uint32_t> PIXEL_FORMAT_STRS {
  { "NV12", V4L2_PIX_FMT_NV12 },
  { "YUYV", V4L2_PIX_FMT_YUYV },
  { "YU12", V4L2_PIX_FMT_YUV420 }
};

class Camera : public FrameInput
{
private:
  uint16_t width_;
  uint16_t height_;

  FileDescriptor camera_fd_;
  std::unique_ptr<MMap_Region> mmap_region_;

  uint32_t pixel_format_;
  v4l2_buffer buffer_info_;
  int type_;

public:
  Camera( const uint16_t width, const uint16_t height,
          const uint32_t pixel_format = V4L2_PIX_FMT_NV12,
          const std::string device = "/dev/video0" );

  ~Camera();

  Optional<RasterHandle> get_next_frame() override;

  uint16_t display_width() { return width_; }
  uint16_t display_height() { return height_; }

  FileDescriptor & fd() { return camera_fd_; }
};

#endif
