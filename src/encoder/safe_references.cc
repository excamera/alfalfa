#include "vp8_raster.hh"
#include "decoder.hh"
#include "encoder.hh"

using namespace std;

SafeReferences::SafeReferences( const uint16_t width, const uint16_t height )
  : last( move ( MutableSafeRasterHandle( width, height ) ) ),
    golden( move ( MutableSafeRasterHandle( width, height ) ) ),
    alternative( move ( MutableSafeRasterHandle( width, height ) ) )
{}

SafeReferences::SafeReferences( const References & references )
  : last( move ( load( references.last ) ) ),
    golden( move ( load( references.golden ) ) ),
    alternative( move ( load( references.alternative ) ) )
{}

const SafeRaster & SafeReferences::get( reference_frame reference_id ) const
{
  switch ( reference_id ) {
  case LAST_FRAME: return last.get();
  case GOLDEN_FRAME: return golden.get();
  case ALTREF_FRAME: return alternative.get();
  default: throw LogicError();
  }
}

MutableSafeRasterHandle SafeReferences::load( const VP8Raster & source )
{
  MutableSafeRasterHandle target( source.display_width(), source.display_height() );
  target.get().copy_raster( source );
  return target;
}
