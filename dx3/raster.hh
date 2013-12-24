#ifndef RASTER_HH
#define RASTER_HH

#include "2d.hh"
#include "modemv_data.hh"

class Component
{
private:
  uint8_t value[ 1 ]; /* allowed to be uninitialized */

public:
  operator uint8_t() const { return value[ 0 ]; }
  Component() {}
  Component( const uint8_t other ) { value[ 0 ] = other; }
  Component & operator=( const uint8_t other ) { value[ 0 ] = other; return *this; }
  void clamp( const int16_t other ) { if ( other < 0 ) { value[ 0 ] = 0; } else if ( other > 255 ) { value[ 0 ] = 255; } else { value[ 0 ] = other; } }
};

/* specialize TwoD because context not necessary for raster of Components */
template<>
template< typename... Targs >
TwoD< Component >::TwoD( const unsigned int width, const unsigned int height, Targs&&... Fargs )
  : width_( width ), height_( height ), storage_()
{
  assert( width > 0 );
  assert( height > 0 );

  storage_.reserve( width * height );

  for ( unsigned int row = 0; row < height; row++ ) {
    for ( unsigned int column = 0; column < width; column++ ) {
      storage_.emplace_back( Fargs... );
    }
  }
}

class Raster
{
public:
  template <unsigned int size>
  struct Block
  {
    TwoDSubRange< Component, size, size > contents;

    Block( const typename TwoD< Block >::Context & c, TwoD< Component > & macroblock_component );

    Component & at( const unsigned int column, const unsigned int row )
    { return contents.at( column, row ); }

    const Component & at( const unsigned int column, const unsigned int row ) const
    { return contents.at( column, row ); }

    const typename TwoD< Block >::Context context;

    template <class PredictionMode>
    void intra_predict( const PredictionMode mb_mode );

    void dc_predict( void );
    void dc_predict_simple( void );
    void v_predict( void );
    void h_predict( void );
    void tm_predict( void );
    void ve_predict( void );
    void he_predict( void );
    void ld_predict( void );
    void rd_predict( void );
    void vr_predict( void );
    void vl_predict( void );
    void hd_predict( void );
    void hu_predict( void );

    struct Predictors {
      typedef TwoDSubRange< Component, size, 1 > Row;
      typedef TwoDSubRange< Component, 1, size > Column;

      static const Row & row127( void );
      static const Column & col129( void );

      const Row above_row;
      const Column left_column;
      const Component & above_left;
      Row above_right_bottom_row; /* non-const so macroblock can fix */
      const Component & above_bottom_right_pixel;
      bool use_row;
      Component above_right( const unsigned int column ) const;

      Component above( const int column ) const;
      Component left( const int row ) const;
      Component east( const int num ) const;

      Predictors( const typename TwoD< Block >::Context & context );
    } predictors;

    Component above( const int column ) const { return predictors.above( column ); }
    Component left( const int column ) const { return predictors.left( column ); }
    Component east( const int column ) const { return predictors.east( column ); }
  };

  using Block4  = Block< 4 >;
  using Block8  = Block< 8 >;
  using Block16 = Block< 16 >;

  struct Macroblock
  {
    Block16 & Y;
    Block8 & U;
    Block8 & V;
    TwoDSubRange< Block4, 4, 4 > Y_sub;
    TwoDSubRange< Block4, 2, 2 > U_sub, V_sub;

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

  const TwoD< Component > & Y( void ) const { return Y_; }
  const TwoD< Component > & U( void ) const { return U_; }
  const TwoD< Component > & V( void ) const { return V_; }

  Macroblock & macroblock( const unsigned int column, const unsigned int row )
  {
    return macroblocks_.at( column, row );
  }

  unsigned int width( void ) const { return width_; }
  unsigned int height( void ) const { return height_; }
  unsigned int display_width( void ) const { return display_width_; }
  unsigned int display_height( void ) const { return display_height_; }
};

#endif /* RASTER_HH */
