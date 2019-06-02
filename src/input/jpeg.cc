/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Copyright 2013-2019 the Alfalfa authors
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

#include <array>
#include <iostream>

#include "jpeg.hh"

using namespace std;

JPEGDecompresser::JPEGDecompresser()
{
  jpeg_std_error( &error_manager_ );
  error_manager_.error_exit = JPEGDecompresser::error;
  decompresser_.err = &error_manager_;
  jpeg_create_decompress( &decompresser_ );
}

JPEGDecompresser::~JPEGDecompresser()
{
  jpeg_destroy_decompress( &decompresser_ );
}

void JPEGDecompresser::error( const j_common_ptr cinfo )
{
  array<char, JMSG_LENGTH_MAX> error_message;
  (*cinfo->err->format_message)( cinfo, error_message.data() );

  throw runtime_error( error_message.data() );
}

void JPEGDecompresser::begin_decoding( const Chunk & chunk )
{
  /* older versions of libjpeg did not use a const pointer
     in jpeg_mem_src's buffer argument */
  jpeg_mem_src( &decompresser_,
                const_cast<uint8_t *>( chunk.buffer() ),
                chunk.size() );

  if ( JPEG_HEADER_OK != jpeg_read_header( &decompresser_, true ) ) {
    throw runtime_error( "invalid JPEG" );
  }

  decompresser_.raw_data_out = true;

  if ( decompresser_.jpeg_color_space != JCS_YCbCr ) {
    throw runtime_error( "not Y'CbCr" );
  }

  if ( decompresser_.num_components != 3 ) {
    throw runtime_error( "not 3 components" );
  }

  if (    decompresser_.comp_info[ 0 ].h_samp_factor != 2
       or decompresser_.comp_info[ 0 ].v_samp_factor != 1
       or decompresser_.comp_info[ 1 ].h_samp_factor != 1
       or decompresser_.comp_info[ 1 ].v_samp_factor != 1
       or decompresser_.comp_info[ 2 ].h_samp_factor != 1
       or decompresser_.comp_info[ 2 ].v_samp_factor != 1 ) {
    throw runtime_error( "not 4:2:2" );
  }

  if ( Y_.initialized() ) {
    if ( Y_.get().height() != height()
         or U_.get().height() != height()
         or V_.get().height() != height()
         or Y_.get().width() != width()
         or U_.get().width() != width() / 2
         or V_.get().width() != width() / 2 ) {
      throw runtime_error( "JPEG size changed" );
    }
  } else {
    Y_.initialize( width(), height() );
    U_.initialize( width() / 2, height() );
    V_.initialize( width() / 2, height() );

    for ( unsigned int i = 0; i < height(); i++ ) {
      Y_rows.emplace_back( &Y_.get().at( 0, i ) );
      U_rows.emplace_back( &U_.get().at( 0, i ) );
      V_rows.emplace_back( &V_.get().at( 0, i ) );
    }
  }
}

unsigned int JPEGDecompresser::width() const
{
  return decompresser_.image_width;
}

unsigned int JPEGDecompresser::height() const
{
  return decompresser_.image_height;
}

void JPEGDecompresser::decode( BaseRaster & r )
{
  if ( r.display_height() != height() or r.display_width() != width() ) {
    throw runtime_error( "size mismatch" );
  }

  jpeg_start_decompress( &decompresser_ );

  while ( decompresser_.output_scanline < decompresser_.output_height ) {
    array<uint8_t **, 3> image { &Y_rows.at( decompresser_.output_scanline ),
        &U_rows.at( decompresser_.output_scanline ),
        &V_rows.at( decompresser_.output_scanline ) };

    const auto decoded = jpeg_read_raw_data( &decompresser_, image.data(), height() );

    if ( decoded != DCTSIZE ) {
      throw runtime_error( "jpeg_read_raw_data returned short read" );
    }
  }

  jpeg_finish_decompress( &decompresser_ );

  memcpy( &r.Y().at( 0, 0 ), &Y_.get().at( 0, 0 ), width() * height() );

  for ( size_t row = 0; row < height() / 2; row++ ) {
    for ( size_t column = 0; column < width() / 2; column++ ) {
      r.U().at( column, row ) = U_.get().at( column, row * 2 );
      r.V().at( column, row ) = V_.get().at( column, row * 2 );
    }
  }
}
