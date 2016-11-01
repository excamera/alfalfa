#include "vp8_raster.hh"
#include "decoder.hh"
#include "encoder.hh"

SafeReferences::SafeReferences( const uint16_t width, const uint16_t height )
  : last_( width, height ), golden_( width, height ),
    alternative_( width, height )
{}

SafeReferences::SafeReferences( const References & references )
  : SafeReferences( references.last.get().width(), references.last.get().height() )
{
  update_all_refs( references );
}

void SafeReferences::update_ref( reference_frame reference_id, RasterHandle reference_raster )
{
  switch ( reference_id ) {
  case LAST_FRAME: last_.copy_raster( reference_raster.get() ); break;
  case GOLDEN_FRAME: golden_.copy_raster( reference_raster.get() ); break;
  case ALTREF_FRAME: alternative_.copy_raster( reference_raster.get() ); break;
  default: throw LogicError();
  }
}

void SafeReferences::update_all_refs( const References & references )
{
  update_ref( LAST_FRAME, references.last );
  update_ref( GOLDEN_FRAME, references.golden );
  update_ref( ALTREF_FRAME, references.alternative );
}

const SafeRaster & SafeReferences::get( reference_frame reference_id ) const
{
  switch ( reference_id ) {
  case LAST_FRAME: return last_;
  case GOLDEN_FRAME: return golden_;
  case ALTREF_FRAME: return alternative_;
  default: throw LogicError();
  }
}
