/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <stdexcept>

#include "ssim.hh"

double ssim( const TwoD<uint8_t> & image __attribute((unused)),
             const TwoD<uint8_t> & other_image __attribute((unused)) )
{
  throw std::runtime_error( "UNIMPLEMENTED: libx264 ssim was removed from Alfalfa to allow BSD release" );
}
