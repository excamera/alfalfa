#ifndef RASTER_HH
#define RASTER_HH

#include "2d.hh"

class Raster
{
public:
  struct Component
  {
    uint8_t value {};
    operator uint8_t() { return value; }
    Component( const TwoD< Component >::Context & ) {} 
    Component & operator=( const uint8_t other ) { value = other; return *this; }
  };

  struct Block
  {
    TwoDSubRange< Component > contents;

    Block( TwoD< Block >::Context & c, TwoD< Component > & macroblock_component );

    Component & at( const unsigned int column, const unsigned int row )
    { return contents.at( column, row ); }
  };

  struct Macroblock
  {
    TwoDSubRange< Component > Y, U, V;
    TwoDSubRange< Block > Y_blocks, U_blocks, V_blocks;

    Macroblock( TwoD< Macroblock >::Context & c, Raster & raster );
  };

private:
  unsigned int width_, height_;
  unsigned int display_width_, display_height_;

  TwoD< Component > Y_ { width_, height_ },
    U_ { width_ / 2, height_ / 2 },
    V_ { width_ / 2, height_ / 2 };

  TwoD< Block > Y_blocks_ { width_ / 4, height_ / 4, Y_ },
    U_blocks_ { width_ / 8, height_ / 8, U_ },
    V_blocks_ { width_ / 8, height_ / 8, V_ };

  TwoD< Macroblock > macroblocks_ { width_ / 16, height_ / 16, *this };

public:
  Raster( const unsigned int macroblock_width, const unsigned int macroblock_height,
	  const unsigned int display_width, const unsigned int display_height );

  TwoD< Component > & Y( void ) { return Y_; }
  TwoD< Component > & U( void ) { return U_; }
  TwoD< Component > & V( void ) { return V_; }

  TwoD< Block > & Y_blocks( void ) { return Y_blocks_; }
  TwoD< Block > & U_blocks( void ) { return U_blocks_; }
  TwoD< Block > & V_blocks( void ) { return V_blocks_; }

  Macroblock & macroblock( const unsigned int column, const unsigned int row )
  {
    return macroblocks_.at( column, row );
  }
};

#endif /* RASTER_HH */
