#ifndef RASTER_HH
#define RASTER_HH

#include "2d.hh"

class Raster
{
private:
  struct Component
  {
    uint8_t value {};
    operator uint8_t() { return value; }
    Component( const TwoD< Component >::Context & ) {} 
    Component & operator=( const uint8_t other ) { value = other; return *this; }
  };

  unsigned int width_, height_;
  unsigned int display_width_, display_height_;

  TwoD< Component > Y_ { width_, height_ };
  TwoD< Component > U_ { width_ / 2, height_ / 2 };
  TwoD< Component > V_ { width_ / 2, height_ / 2 };

public:
  struct Macroblock
  {
    TwoDSubRange< Component > Y_, U_, V_;
  };

  Raster( const unsigned int width, const unsigned int height,
	  const unsigned int display_width, const unsigned int display_height );

  Macroblock macroblock( const unsigned int macroblock_column, const unsigned int macroblock_row );
};

#endif /* RASTER_HH */
