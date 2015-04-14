#ifndef PLAYER_HH
#define PLAYER_HH

#include <string>

#include "ivf.hh"
#include "decoder.hh"

class Player
{
private:
  IVF file_;

  uint16_t width_, height_;

  Decoder decoder_ { width_, height_ };

  unsigned int frame_no_ = { 0 };

public:
  Player( const std::string & file_name );

  bool next_shown_frame( RasterHandle & raster, bool preloop = false );

  vector< uint8_t > get_continuation( RasterHandle & source_raster );

  RasterHandle new_raster( void );

  uint16_t width( void ) const
  {
    return width_;
  }

  uint16_t height( void ) const
  {
    return height_;
  }

};

#endif
