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
    struct Block
    {
      TwoDSubRange< Component > contents;

      Block( TwoD< Block >::Context & c, TwoDSubRange< Component > & macroblock_component );

      Component & at( const unsigned int column, const unsigned int row ) { return contents.at( column, row ); }
    };

    TwoDSubRange< Component > Y, U, V;
    TwoD< Block > Y_blocks, U_blocks, V_blocks;

    Macroblock( TwoD< Macroblock >::Context & c, Raster & raster );
  };

private:
  TwoD< Macroblock > macroblocks_;

public:
  Raster( const unsigned int width, const unsigned int height,
	  const unsigned int display_width, const unsigned int display_height );

  TwoD< Component > & Y( void ) { return Y_; }
  TwoD< Component > & U( void ) { return U_; }
  TwoD< Component > & V( void ) { return V_; }

  Macroblock & macroblock( const unsigned int column, const unsigned int row )
  {
    return macroblocks_.at( column, row );
  }
};

#endif /* RASTER_HH */
