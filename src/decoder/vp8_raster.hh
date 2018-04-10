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

#ifndef VP8_RASTER_H
#define VP8_RASTER_H

#include <iostream>

#include "config.h"
#include "raster.hh"

#ifdef HAVE_SSE2
#include "predictor_sse.hh"
#endif

class MotionVector;

template <class integer>
static inline uint8_t clamp255( const integer value )
{
  if ( value < 0 ) return 0;
  if ( value > 255 ) return 255;
  return value;
}

class SafeRaster;

class VP8Raster : public BaseRaster
{
public:
  template <unsigned int size>
  class Block
  {
  public:
    typedef TwoDSubRange<uint8_t, size, 1> Row;
    typedef TwoDSubRange<uint8_t, 1, size> Column;

  private:
    typedef TwoDSubRange<uint8_t, size, size> BlockSubRange;

    BlockSubRange contents_;
    const TwoD<uint8_t> & raster_component_;

    unsigned int column_, row_;

    struct Predictors {
    private:
      const static size_t OFFSET = 16;
      uint8_t above_storage[ OFFSET + size * 2 ] __attribute__((aligned( 16 )));

    public:
      uint8_t * above { above_storage + OFFSET };
      uint8_t left[ size ] __attribute__((aligned( 16 )));

      uint8_t east( size_t i ) const { return ( i <= 3 ) ? left[ 3 - i ] : above[ i - 5 ]; }
    };

    void dc_predict( const Predictors &, BlockSubRange & output ) const;
    void dc_predict_simple( const Predictors &, BlockSubRange & output ) const;
    void vertical_predict( const Predictors &, BlockSubRange & output ) const;
    void horizontal_predict( const Predictors &, BlockSubRange & output ) const;
    void true_motion_predict( const Predictors &, BlockSubRange & output ) const;
    void vertical_smoothed_predict( const Predictors &, BlockSubRange & output ) const;
    void horizontal_smoothed_predict( const Predictors &, BlockSubRange & output ) const;
    void left_down_predict( const Predictors &, BlockSubRange & output ) const;
    void right_down_predict( const Predictors &, BlockSubRange & output ) const;
    void vertical_right_predict( const Predictors &, BlockSubRange & output ) const;
    void vertical_left_predict( const Predictors &, BlockSubRange & output ) const;
    void horizontal_down_predict( const Predictors &, BlockSubRange & output ) const;
    void horizontal_up_predict( const Predictors &, BlockSubRange & output ) const;

  public:
    unsigned int column() const { return column_; }
    unsigned int row() const { return row_; }

    Block( const unsigned int column, const unsigned int row, TwoD<uint8_t> & raster_component );

    uint8_t & at( const unsigned int column, const unsigned int row )
    { return contents_.at( column, row ); }

    const uint8_t & at( const unsigned int column, const unsigned int row ) const
    { return contents_.at( column, row ); }

    unsigned int stride( void ) const { return contents_.stride(); }

    const decltype(contents_) & contents( void ) const { return contents_; }
    decltype(contents_) & mutable_contents( void ) { return contents_; }

    Predictors predictors() const;

    template <class PredictionMode>
    void intra_predict( const PredictionMode mb_mode ) { intra_predict( mb_mode, predictors(), this->contents_ ); }

    template <class PredictionMode>
    void intra_predict( const PredictionMode mb_mode,
                        const Predictors & predictors,
                        TwoDSubRange<uint8_t, size, size> & output ) const;

    void inter_predict( const MotionVector & mv,
                        const TwoD<uint8_t> & reference ) { inter_predict( mv, reference, this->contents_ ); }

    void inter_predict( const MotionVector & mv,
                        const TwoD<uint8_t> & reference,
                        TwoDSubRange<uint8_t, size, size> & output ) const;

    /* for encoder use */
    void inter_predict( const MotionVector & mv,
                        const TwoD<uint8_t> & reference,
                        TwoD<uint8_t> & output ) const;

    void inter_predict( const MotionVector & mv,
                        const SafeRaster & reference,
                        TwoDSubRange<uint8_t, size, size> & output ) const;

    template <class ReferenceType>
    void safe_inter_predict( const MotionVector & mv,
                             const ReferenceType & reference,
                             const int source_column, const int source_row,
                             TwoDSubRange<uint8_t, size, size> & output ) const;

    void unsafe_inter_predict( const MotionVector & mv,
                               const TwoD<uint8_t> & reference,
                               const int source_column, const int source_row,
                               TwoDSubRange<uint8_t, size, size> & output ) const;

#ifdef HAVE_SSE2
    static void sse_horiz_inter_predict( const uint8_t * src, const unsigned int pixels_per_line,
                                         const uint8_t * dst, const unsigned int dst_pitch,
                                         const unsigned int dst_height, const unsigned int filter_idx );

    static void sse_vert_inter_predict( const uint8_t * src, const unsigned int pixels_per_line,
                                        const uint8_t * dst, const unsigned int dst_pitch,
                                        const unsigned int dst_height, const unsigned int filter_idx );
#endif

    static constexpr unsigned int dimension { size };

    SafeArray<SafeArray<int16_t, size>, size> operator-( const Block & other ) const;

    friend std::ostream &operator<<( std::ostream & output, const Block<size> & block )
    {
      for ( size_t i = 0; i < size; i++ ) {
        for ( size_t j = 0; j < size; j++ ) {
          std::cout << static_cast<int>( block.contents().at( i, j ) );
          if ( j != size - 1 or i != size - 1 ) std::cout << ", ";
        }
      }

      return output;
    }
  };

  using Block4  = Block<4>;
  using Block8  = Block<8>;
  using Block16 = Block<16>;

  struct Macroblock
  {
    Block16 Y;
    Block8 U;
    Block8 V;
    SafeArray<Block4, 16> Y_sub;
    SafeArray<Block4, 4> U_sub, V_sub;

    Macroblock( const TwoD<Macroblock>::Context & c, VP8Raster & raster );

    Macroblock( const unsigned int column, const unsigned int row, VP8Raster & raster );

    bool operator==( const Macroblock & other ) const
    {
      return Y.contents() == other.Y.contents()
         and U.contents() == other.U.contents()
         and V.contents() == other.V.contents();
    }

    bool operator!=( const Macroblock & other ) const
    {
      return not operator==( other );
    }

    Macroblock( Macroblock && other );

    Macroblock & operator=( const Macroblock & other ) = delete;

    Block4 & Y_sub_at( const unsigned int column, const unsigned int row ) {
      return Y_sub.at( row * 4 + column );
    }

    Block4 & U_sub_at( const unsigned int column, const unsigned int row ) {
      return U_sub.at( row * 2 + column );
    }

    Block4 & V_sub_at( const unsigned int column, const unsigned int row ) {
      return V_sub.at( row * 2 + column );
    }

    const Block4 & Y_sub_at( const unsigned int column, const unsigned int row ) const {
      return Y_sub.at( row * 4 + column );
    }

    const Block4 & U_sub_at( const unsigned int column, const unsigned int row ) const {
      return U_sub.at( row * 2 + column );
    }

    const Block4 & V_sub_at( const unsigned int column, const unsigned int row ) const {
      return V_sub.at( row * 2 + column );
    }

    template <class lambda> void Y_sub_forall_ij( const lambda & f ) { forall_ij<4, 4>( Y_sub, f ); }
    template <class lambda> void U_sub_forall_ij( const lambda & f ) { forall_ij<2, 2>( U_sub, f ); }
    template <class lambda> void V_sub_forall_ij( const lambda & f ) { forall_ij<2, 2>( V_sub, f ); }

    template <unsigned int s_width, unsigned int s_height, class lambda>
    void forall_ij( SafeArray<Block4, s_width * s_height> & sub, const lambda & f ) const
    {
      for ( unsigned int row = 0; row < s_height; row++ ) {
        for ( unsigned int column = 0; column < s_width; column++ ) {
          f( sub.at( row * s_width + column ), column, row );
        }
      }
    }
  };

  struct ConstMacroblock
  {
  private:
    Macroblock macroblock_;

  public:
    ConstMacroblock( const unsigned int column, const unsigned int row, const VP8Raster & raster )
      : macroblock_( column, row, const_cast<VP8Raster &>( raster ) )
    {}

    ConstMacroblock( ConstMacroblock && other )
    : macroblock_( std::move( const_cast<Macroblock &> ( other.macroblock_ ) ) )
    {}

    const Macroblock & macroblock() const { return macroblock_; }

    const Block16 & Y() const { return macroblock_.Y; }
    const Block8 & U() const { return macroblock_.U; }
    const Block8 & V() const { return macroblock_.V; }

    const Block4 & Y_sub_at( const unsigned int column, const unsigned int row ) const {
      return macroblock_.Y_sub_at( column, row );
    }

    const Block4 & U_sub_at( const unsigned int column, const unsigned int row ) const {
      return macroblock_.U_sub_at( column, row );
    }

    const Block4 & V_sub_at( const unsigned int column, const unsigned int row ) const {
      return macroblock_.V_sub_at( column, row );
    }
  };

public:
  VP8Raster( const unsigned int display_width, const unsigned int display_height );

  Macroblock macroblock( const unsigned int column, const unsigned int row )
  {
    return { column, row, *this };
  }

  ConstMacroblock macroblock( const unsigned int column, const unsigned int row ) const
  {
    return { column, row, *this };
  }

  static unsigned int macroblock_dimension( const unsigned int num ) { return ( num + 15 ) / 16; }

  template <class lambda>
  void macroblocks_forall_ij( const lambda & f ) const
  {
    for ( unsigned int row = 0; row < height_ / 16; row++ ) {
      for ( unsigned int column = 0; column < width_ / 16; column++ ) {
        f( macroblock( column, row ), column, row );
      }
    }
  }

  template <class lambda>
  void macroblocks_forall_ij( const lambda & f )
  {
    for ( unsigned int row = 0; row < height_ / 16; row++ ) {
      for ( unsigned int column = 0; column < width_ / 16; column++ ) {
        f( macroblock( column, row ), column, row );
      }
    }
  }
};

class EdgeExtendedRaster
{
private:
  const TwoD<uint8_t> & master_;

public:
  EdgeExtendedRaster( const TwoD<uint8_t> & master )
    : master_( master ) {}

  uint8_t at( const int column, const int row ) const
  {
    int bounded_column = column;
    if ( bounded_column < 0 ) bounded_column = 0;
    if ( bounded_column > int(master_.width() - 1) ) bounded_column = master_.width() - 1;

    int bounded_row = row;
    if ( bounded_row < 0 ) bounded_row = 0;
    if ( bounded_row > int(master_.height() - 1) ) bounded_row = master_.height() - 1;

    return master_.at( bounded_column, bounded_row );
  }
};

class SafeRaster
{
private:
  TwoD<uint8_t> safe_Y_;
  
  uint16_t display_width_;
  uint16_t display_height_;

public:
  static const size_t MARGIN_WIDTH = 256;

  SafeRaster( uint16_t width, uint16_t height );
  SafeRaster( const VP8Raster & source );

  uint16_t display_width() const { return display_width_; }
  uint16_t display_height() const { return display_height_; }

  /* Copies the Y plane of the given raster to the target buffer and automatically
     does the edge extension. */
  void copy_raster( const VP8Raster & source );

  const uint8_t & at( int row, int column ) const;

  unsigned int stride() const;
};

#endif //
