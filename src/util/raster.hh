/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef RASTER_HH
#define RASTER_HH

#include <vector>

#include "2d.hh"
#include "safe_array.hh"
#include "chunk.hh"

/* For an array of pixels, context and separate construction not necessary */
template<>
template< typename... Targs >
TwoDStorage<uint8_t>::TwoDStorage( const unsigned int width, const unsigned int height, Targs&&... Fargs )
  : width_( width ), height_( height ), storage_( width * height, Fargs... )
{
  assert( width > 0 );
  assert( height > 0 );
}

class BaseRaster
{
protected:
  uint16_t display_width_, display_height_;
  uint16_t width_, height_;

  TwoD< uint8_t > Y_ { width_, height_ },
    U_ { width_ / 2, height_ / 2 },
    V_ { width_ / 2, height_ / 2 };

  size_t raw_hash( void ) const;

public:
  BaseRaster( const uint16_t display_width, const uint16_t display_height,
    const uint16_t width, const uint16_t height );

  TwoD< uint8_t > & Y( void ) { return Y_; }
  TwoD< uint8_t > & U( void ) { return U_; }
  TwoD< uint8_t > & V( void ) { return V_; }

  const TwoD< uint8_t > & Y( void ) const { return Y_; }
  const TwoD< uint8_t > & U( void ) const { return U_; }
  const TwoD< uint8_t > & V( void ) const { return V_; }

  uint16_t width( void ) const { return width_; }
  uint16_t height( void ) const { return height_; }
  uint16_t display_width( void ) const { return display_width_; }
  uint16_t display_height( void ) const { return display_height_; }

  uint16_t chroma_display_width() const { return (1 + display_width_) / 2; }
  uint16_t chroma_display_height() const { return (1 + display_height_) / 2; }

  // SSIM as determined by libx264
  double quality( const BaseRaster & other ) const;

  bool operator==( const BaseRaster & other ) const;
  bool operator!=( const BaseRaster & other ) const;

  void copy_from( const BaseRaster & other );

  std::vector<Chunk> display_rectangle_as_planar() const;
  void dump( FILE * file ) const; /* only used for debugging */
};

#endif /* RASTER_HH */
