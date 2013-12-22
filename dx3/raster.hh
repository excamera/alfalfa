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

  template <unsigned int size>
  struct Block
  {
    TwoDSubRange< Component > contents;

    Block( const typename TwoD< Block >::Context & c, TwoD< Component > & macroblock_component );

    Component & at( const unsigned int column, const unsigned int row )
    { return contents.at( column, row ); }

    const typename TwoD< Block >::Context context;
  };

  using Block4  = Block< 4 >;
  using Block8  = Block< 8 >;
  using Block16 = Block< 16 >;

  struct Macroblock
  {
    TwoDSubRange< Block16 > Y;
    TwoDSubRange< Block8 > U, V;
    TwoDSubRange< Block4 > Y_sub, U_sub, V_sub;

    Macroblock( const TwoD< Macroblock >::Context & c, Raster & raster );
  };

private:
  unsigned int width_, height_;
  unsigned int display_width_, display_height_;

  TwoD< Component > Y_ { width_, height_ },
    U_ { width_ / 2, height_ / 2 },
    V_ { width_ / 2, height_ / 2 };

  TwoD< Block4 > Y_subblocks_ { width_ / 4, height_ / 4, Y_ },
    U_subblocks_ { width_ / 8, height_ / 8, U_ },
    V_subblocks_ { width_ / 8, height_ / 8, V_ };

  TwoD< Block16 > Y_bigblocks_ { width_ / 16, height_ / 16, Y_ };
  TwoD< Block8 >  U_bigblocks_ { width_ / 16, height_ / 16, U_ };
  TwoD< Block8 >  V_bigblocks_ { width_ / 16, height_ / 16, V_ };

  TwoD< Macroblock > macroblocks_ { width_ / 16, height_ / 16, *this };

  friend class Macroblock;

public:
  Raster( const unsigned int macroblock_width, const unsigned int macroblock_height,
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
