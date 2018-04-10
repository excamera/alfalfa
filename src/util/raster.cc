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

#include <boost/functional/hash.hpp>
#include <cstdio>

#include "exception.hh"
#include "raster.hh"
#include "ssim.hh"

using namespace std;

BaseRaster::BaseRaster( const uint16_t display_width, const uint16_t display_height,
  const uint16_t width, const uint16_t height)
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

vector<Chunk> BaseRaster::display_rectangle_as_planar() const
{
  vector<Chunk> ret;

  /* write Y */
  for ( uint16_t row = 0; row < display_height(); row++ ) {
    ret.emplace_back( &Y().at( 0, row ), display_width() );
  }

  /* write U */
  for ( uint16_t row = 0; row < chroma_display_height(); row++ ) {
    ret.emplace_back( &U().at( 0, row ), chroma_display_width() );
  }

  /* write V */
  for ( uint16_t row = 0; row < chroma_display_height(); row++ ) {
    ret.emplace_back( &V().at( 0, row ), chroma_display_width() );
  }

  return ret;
}

void BaseRaster::dump( FILE * file ) const
{
  for ( const auto & chunk : display_rectangle_as_planar() ) {
    if ( 1 != fwrite( chunk.buffer(), chunk.size(), 1, file ) ) {
      throw runtime_error( "fwrite returned short write" );
    }
  }
}
