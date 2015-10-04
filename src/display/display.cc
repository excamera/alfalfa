#include "exception.hh"
#include "display.hh"

using namespace std;

const std::string VideoDisplay::shader_source_scale_from_pixel_coordinates
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
        uv_texcoord = vec2( chroma_texcoord );
      }
    )";

const std::string VideoDisplay::shader_source_passthrough_texture
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
        float fY = texture(yTex, raw_position).x * 1.1643828125;
        float fU = texture(uTex, uv_texcoord).x;
        float fV = texture(vTex, uv_texcoord).x;

        outColor = vec4(
          fY + 1.59602734375 * fV - 0.87078515625,
          fY - 0.39176171875 * fU - 0.81296875 * fV + 0.52959375,
          fY + 2.017234375   * fU - 1.081390625,
          1.0
        );
      }
    )";

VideoDisplay::CurrentContextWindow::CurrentContextWindow( const unsigned int width,
  const unsigned int height,
  const string & title)
  : window_( width, height, title )
{
  window_.make_context_current( true );
}

VideoDisplay::VideoDisplay( const BaseRaster & raster )
  : display_width_( raster.display_width() ),
    display_height_( raster.display_height() ),
    width_( raster.width() ),
    height_( raster.height() ),
    current_context_window_( display_width_, display_height_, "VP8 Player" ),
    Y_ ( width_, height_),
    U_ ( width_ / 2, height_ / 2 ),
    V_ ( width_ / 2, height_ / 2 )
{
  texture_shader_program_.attach( scale_from_pixel_coordinates_ );
  texture_shader_program_.attach( passthrough_texture_ );
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

void VideoDisplay::resize( const std::pair<unsigned int, unsigned int> & target_size )
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
    { 0, target_size_y, 0, target_size_y / 2 },
    { target_size_x, target_size_y, target_size_x / 2 + xoffset, target_size_y / 2 },
    { target_size_x, 0 , target_size_x / 2 + xoffset, 0 }
  };

  texture_shader_array_object_.bind();
  ArrayBuffer::bind( screen_corners_ );
  ArrayBuffer::load( corners, GL_STATIC_DRAW );

  /*Y_.resize( target_size.first, target_size.second );
  U_.resize( target_size.first, target_size.second );
  V_.resize( target_size.first, target_size.second );*/

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
  ArrayBuffer::bind( screen_corners_ );
  texture_shader_array_object_.bind();
  texture_shader_program_.use();
  glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );

  current_context_window_.window_.swap_buffers();
}
