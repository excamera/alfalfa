#include "exception.hh"
#include "raster.hh"

BaseRaster::BaseRaster( const unsigned int display_width, const unsigned int display_height,
  const unsigned int width, const unsigned int height)
  : display_width_( display_width ), display_height_( display_height ),
    width_( width ), height_( height )
{
  if ( display_width_ > width_ ) {
    throw Invalid( "display_width is greater than width." );
  }

  if ( display_height_ > height_ ) {
    throw Invalid( "display_height is greater than height." );
  }
}

size_t BaseRaster::raw_hash( void ) const
{
  size_t hash_val = 0;

  boost::hash_range( hash_val, Y_.begin(), Y_.end() );
  boost::hash_range( hash_val, U_.begin(), U_.end() );
  boost::hash_range( hash_val, V_.begin(), V_.end() );

  return hash_val;
}

double BaseRaster::quality( const BaseRaster & other ) const
{
  return ssim( Y(), other.Y() );
}

bool BaseRaster::operator==( const BaseRaster & other ) const
{
  return (Y_ == other.Y_) and (U_ == other.U_) and (V_ == other.V_);
}

bool BaseRaster::operator!=( const BaseRaster & other ) const
{
  return not operator==( other );
}

void BaseRaster::copy_from( const BaseRaster & other )
{
  Y_.copy_from( other.Y_ );
  U_.copy_from( other.U_ );
  V_.copy_from( other.V_ );
}

void BaseRaster::dump( FILE * file ) const
{
  for ( unsigned int row = 0; row < display_height(); row++ ) {
    fwrite( &Y().at( 0, row ), display_width(), 1, file );
  }
  for ( unsigned int row = 0; row < (1 + display_height()) / 2; row++ ) {
    fwrite( &U().at( 0, row ), (1 + display_width()) / 2, 1, file );
  }
  for ( unsigned int row = 0; row < (1 + display_height()) / 2; row++ ) {
    fwrite( &V().at( 0, row ), (1 + display_width()) / 2, 1, file );
  }
}

void BaseRaster::dump( int fd ) const
{
  for ( unsigned int row = 0; row < display_height(); row++ ) {
    SystemCall( "write raster", write( fd, &Y().at( 0, row ), display_width() ) );
  }
  for ( unsigned int row = 0; row < (1 + display_height()) / 2; row++ ) {
    SystemCall( "write raster", write( fd, &U().at( 0, row ), (1 + display_width()) / 2 ) );
  }
  for ( unsigned int row = 0; row < (1 + display_height()) / 2; row++ ) {
    SystemCall( "write raster", write( fd, &V().at( 0, row ), (1 + display_width()) / 2 ) );
  }
}
