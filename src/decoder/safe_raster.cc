#include "vp8_raster.hh"
#include "raster_handle.hh"
#include "decoder.hh"

SafeRaster::SafeRaster( uint16_t width, uint16_t height )
  : safe_Y_( width + MARGIN_WIDTH * 2, height + MARGIN_WIDTH * 2 ),
    display_width_( width ), display_height_( height )
{}

SafeRaster::SafeRaster( const VP8Raster & source )
  : SafeRaster( source.width(), source.height() )
{
  copy_raster( source );
}

void SafeRaster::copy_raster( const VP8Raster & source )
{
  const TwoD<uint8_t> & original_Y = source.Y();
  const uint16_t original_width = original_Y.width();
  const uint16_t original_height = original_Y.height();

  /*
       1   |   2   |   3
    -----------------------
       4   |   5   |   6
    -----------------------
       7   |   8   |   9

    The original image goes into 5.
  */

  for ( size_t nrow = 0; nrow < safe_Y_.height(); nrow++ ) {
    if ( nrow < MARGIN_WIDTH ) {
      memset( &safe_Y_.at( 0, nrow ),
              original_Y.at( 0, 0 ),
              MARGIN_WIDTH ); // (1)

      memcpy( &safe_Y_.at( MARGIN_WIDTH, nrow ),
              &original_Y.at( 0, 0 ),
              original_width ); // (2)

      memset( &safe_Y_.at( MARGIN_WIDTH + original_width, nrow ),
              original_Y.at( original_width - 1, 0 ),
              MARGIN_WIDTH ); // (3)
    }
    else if ( nrow >= MARGIN_WIDTH + original_Y.height() ) {
      memset( &safe_Y_.at( 0, nrow ),
              original_Y.at( 0, original_height - 1 ),
              MARGIN_WIDTH ); // (7)

      memcpy( &safe_Y_.at( MARGIN_WIDTH, nrow ),
              &original_Y.at( 0, original_height - 1 ),
              original_width ); // (8)

      memset( &safe_Y_.at( MARGIN_WIDTH + original_width, nrow ),
              original_Y.at( original_width - 1, original_height - 1 ),
              MARGIN_WIDTH ); // (9)
    }
    else {
      memset( &safe_Y_.at( 0, nrow ),
              original_Y.at( 0, nrow - MARGIN_WIDTH ),
              MARGIN_WIDTH ); // (4)

      memcpy( &safe_Y_.at( MARGIN_WIDTH, nrow ),
              &original_Y.at( 0, nrow - MARGIN_WIDTH ),
              original_width ); // (5)

      memset( &safe_Y_.at( MARGIN_WIDTH + original_width, nrow ),
              original_Y.at( original_width - 1, nrow - MARGIN_WIDTH ),
              MARGIN_WIDTH ); // (6)
    }
  }
}

const uint8_t & SafeRaster::at( int  column, int row ) const
{
  assert( (int)MARGIN_WIDTH + column >= 0 and (int)MARGIN_WIDTH + row >= 0 );
  return safe_Y_.at( (int)MARGIN_WIDTH + column, (int)MARGIN_WIDTH + row );
}

unsigned int SafeRaster::stride() const
{
  return safe_Y_.width();
}
