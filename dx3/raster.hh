#ifndef RASTER_HH
#define RASTER_HH

#include "2d.hh"
#include "modemv_data.hh"

class Raster
{
public:
  struct Component
  {
    uint8_t value {};
    operator uint8_t() const { return value; }
    Component( const TwoD< Component >::Context &, const uint8_t s_value = 0 ) : value( s_value ) {} 
    Component( const uint8_t other ) : value( other ) {}
    Component & operator=( const uint8_t other ) { value = other; return *this; }
    void clamp( const int16_t other ) { if ( other < 0 ) { value = 0; } else if ( other > 255 ) { value = 255; } else { value = other; } }
  };

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

  Macroblock & macroblock( const unsigned int column, const unsigned int row )
  {
    return macroblocks_.at( column, row );
  }
};

#endif /* RASTER_HH */
