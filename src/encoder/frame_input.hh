/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef FRAME_INPUT_HH
#define FRAME_INPUT_HH

#include "raster_handle.hh"
#include "optional.hh"

class FrameInput
{
public:
  virtual Optional<RasterHandle> get_next_frame() = 0;
  virtual uint16_t display_width() = 0;
  virtual uint16_t display_height() = 0;
  virtual ~FrameInput() = default;
};

#endif /* FRAME_INPUT_HH */
