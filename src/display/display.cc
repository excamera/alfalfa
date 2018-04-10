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

#include "exception.hh"
#include "display.hh"

using namespace std;

const string VideoDisplay::shader_source_scale_from_pixel_coordinates
= R"( #version 130

      uniform uvec2 window_size;

      in vec2 position;
      in vec2 chroma_texcoord;
      out vec2 raw_position;
      out vec2 uv_texcoord;

      void main()
      {
        gl_Position = vec4( 2 * position.x / window_size.x - 1.0,
                            1.0 - 2 * position.y / window_size.y, 0.0, 1.0 );
        raw_position = vec2( position.x, position.y );
        uv_texcoord = vec2( chroma_texcoord.x, chroma_texcoord.y );
      }
    )";

/* octave> 255 * inv([219*[.7152 .0722 .2126]'
                      224*[-0.38542789394266 .5 -0.11457210605734]'
                      224*[-0.454152908305817 -0.0458470916941834 .5]']') */

/* switched to SMPTE 170M matrix (1/21/2017)
>> 255 * inv([219*[.587 .114 .299]' 224*[-.331 .500 -.169]' 224*[-.419 -.081 .5]']')
ans =

      1.16438356164384    -0.391260370716072    -0.813004933873461
      1.16438356164384      2.01741475897078   0.00112725996069348
      1.16438356164384  -0.00105499970680283      1.59567019581339
*/

const string VideoDisplay::shader_source_ycbcr
= R"( #version 130
      #extension GL_ARB_texture_rectangle : enable

      precision mediump float;

      uniform sampler2DRect yTex;
      uniform sampler2DRect uTex;
      uniform sampler2DRect vTex;

      in vec2 uv_texcoord;
      in vec2 raw_position;
      out vec4 outColor;

      void main()
      {
        float fY = texture(yTex, raw_position).x;
        float fCb = texture(uTex, uv_texcoord).x;
        float fCr = texture(vTex, uv_texcoord).x;

        outColor = vec4(
          max(0, min(1.0, 1.16438356164384 * (fY - 0.06274509803921568627) + 1.59567019581339  * (fCr - 0.50196078431372549019))),
          max(0, min(1.0, 1.16438356164384 * (fY - 0.06274509803921568627) - 0.391260370716072 * (fCb - 0.50196078431372549019) - 0.813004933873461 * (fCr - 0.50196078431372549019))),
          max(0, min(1.0, 1.16438356164384 * (fY - 0.06274509803921568627) + 2.01741475897078  * (fCb - 0.50196078431372549019))),
          1.0
        );
      }
    )";

VideoDisplay::CurrentContextWindow::CurrentContextWindow( const unsigned int width,
  const unsigned int height, const string & title, const bool fullscreen )
  : window_( width, height, title, fullscreen )
{
  window_.make_context_current( true );
}

VideoDisplay::VideoDisplay( const BaseRaster & raster, const bool fullscreen )
  : display_width_( raster.display_width() ),
    display_height_( raster.display_height() ),
    width_( raster.width() ),
    height_( raster.height() ),
    current_context_window_( display_width_, display_height_, "VP8 Player", fullscreen ),
    Y_ ( width_, height_),
    U_ ( width_ / 2, height_ / 2 ),
    V_ ( width_ / 2, height_ / 2 )
{
  texture_shader_program_.attach( scale_from_pixel_coordinates_ );
  texture_shader_program_.attach( ycbcr_shader_ );
  texture_shader_program_.link();
  glCheck( "after linking texture shader program" );

  texture_shader_array_object_.bind();
  ArrayBuffer::bind( screen_corners_ );
  glVertexAttribPointer( texture_shader_program_.attribute_location( "position" ),
    2, GL_FLOAT, GL_FALSE, sizeof( VertexObject ), 0 );
  glEnableVertexAttribArray( texture_shader_program_.attribute_location( "position" ) );

  glVertexAttribPointer( texture_shader_program_.attribute_location( "chroma_texcoord" ),
    2, GL_FLOAT, GL_FALSE, sizeof( VertexObject ), (const void *)( 2 * sizeof( float ) ) );
  glEnableVertexAttribArray( texture_shader_program_.attribute_location( "chroma_texcoord" ) );

  glfwSwapInterval(1);

  Y_.bind( GL_TEXTURE0 );
  U_.bind( GL_TEXTURE1 );
  V_.bind( GL_TEXTURE2 );

  const pair<unsigned int, unsigned int> window_size = window().size();
  resize( window_size );

  glCheck( "" );
}

void VideoDisplay::resize( const pair<unsigned int, unsigned int> & target_size )
{
  glViewport( 0, 0, target_size.first, target_size.second );

  texture_shader_program_.use();
  glUniform2ui( texture_shader_program_.uniform_location( "window_size" ),
                target_size.first, target_size.second );

  glUniform1i( texture_shader_program_.uniform_location( "yTex" ), 0);
  glUniform1i( texture_shader_program_.uniform_location( "uTex" ), 1);
  glUniform1i( texture_shader_program_.uniform_location( "vTex" ), 2);

  const float xoffset = 0.25;

  float target_size_x = target_size.first;
  float target_size_y = target_size.second;

  vector<VertexObject> corners = {
    { 0, 0, xoffset, 0},
    { 0, target_size_y, xoffset, target_size_y / 2 },
    { target_size_x, target_size_y, target_size_x / 2 + xoffset, target_size_y / 2 },
    { target_size_x, 0 , target_size_x / 2 + xoffset, 0 }
  };

  texture_shader_array_object_.bind();
  ArrayBuffer::bind( screen_corners_ );
  ArrayBuffer::load( corners, GL_STATIC_DRAW );

  glCheck( "after resizing ");
}

void VideoDisplay::draw( const BaseRaster & raster )
{
  if ( width_ != raster.width() or height_ != raster.height() ) {
    throw Invalid( "inconsistent raster dimensions." );
  }

  Y_.load( raster.Y() );
  U_.load( raster.U() );
  V_.load( raster.V() );

  repaint();
}

void VideoDisplay::repaint( void )
{
  pair<unsigned int, unsigned int> window_size = current_context_window_.window_.window_size();

  if ( window_size.first != display_width_ or window_size.second != display_height_ ) {
    display_width_ = window_size.first;
    display_height_ = window_size.second;
    resize( make_pair( display_width_, display_height_ ) );
  }

  ArrayBuffer::bind( screen_corners_ );
  texture_shader_array_object_.bind();
  texture_shader_program_.use();
  glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );

  current_context_window_.window_.swap_buffers();
}
