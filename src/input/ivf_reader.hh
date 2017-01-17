/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef IVF_READER_HH
#define IVF_READER_HH

#include "frame_input.hh"
#include "player.hh"

class IVFReader : public FrameInput
{
private:
  FilePlayer player_;

public:
  IVFReader( const std::string & filename );
  Optional<RasterHandle> get_next_frame();
  uint16_t display_width() override { return player_.width(); }
  uint16_t display_height() override { return player_.height(); }
};

#endif /* IVF_READER_HH */
