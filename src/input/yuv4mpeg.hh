/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef YUV4MPEG_HH
#define YUV4MPEG_HH

#include <string>

#include "frame_input.hh"
#include "exception.hh"
#include "raster_handle.hh"
#include "file_descriptor.hh"
#include "vp8_raster.hh"

class YUV4MPEGHeader
{
public:
  enum InterlacingMode { PROGRESSIVE, TOP_FIELD_FIRST, BOTTOM_FIELD_FIRST,
                         MIXED_MODES };

  enum ColorSpace { C420jpeg, C420paldv, C420, C422, C444 };

  YUV4MPEGHeader();
  YUV4MPEGHeader( const BaseRaster &rh );

  uint16_t width;
  uint16_t height;
  uint16_t fps_numerator;
  uint16_t fps_denominator;
  uint16_t pixel_aspect_ratio_numerator;
  uint16_t pixel_aspect_ratio_denominator;
  InterlacingMode interlacing_mode;
  ColorSpace color_space;

  size_t frame_length();
  size_t y_plane_length();
  size_t uv_plane_length();

  std::string to_string();
};

class YUV4MPEGReader : public FrameInput
{
private:
  YUV4MPEGHeader header_;
  FileDescriptor fd_;

  static std::pair< size_t, size_t > parse_fraction( const std::string & fraction_str );

public:
  YUV4MPEGReader( FileDescriptor && fd );
  YUV4MPEGReader( const std::string & filename );
  Optional<RasterHandle> get_next_frame() override;

  uint16_t display_width() override { return header_.width; }
  uint16_t display_height() override { return header_.height; }

  size_t frame_length() { return header_.frame_length(); }
  size_t y_plane_length() { return header_.y_plane_length(); }
  size_t uv_plane_length() { return header_.uv_plane_length(); }

  YUV4MPEGHeader header() const { return header_; }

  FileDescriptor & fd() { return fd_; }
};

class YUV4MPEGFrameWriter
{
public:
  static void write( const BaseRaster &rh, FileDescriptor &fd );
};

#endif /* YUV4MPEG_HH */
