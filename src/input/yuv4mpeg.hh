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
