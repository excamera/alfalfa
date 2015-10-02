#include "exception.hh"
#include "display.hh"

VideoDisplay::CurrentContextWindow::CurrentContextWindow( const unsigned int width,
  const unsigned int height,
  const string & title)
  : window_( width, height, title )
{
  window_.make_context_current( true );
  glfwSwapInterval(1);
}

VideoDisplay::VideoDisplay( const BaseRaster & raster )
  : display_width_( raster.display_width() ),
    display_height_( raster.display_height() ),
    width_( raster.width() ),
    height_( raster.height() ),
    current_context_window_( display_width_, display_height_, "VP8 Player" ),
    Y_ ( GL_TEXTURE0, width_, height_),
    U_ ( GL_TEXTURE1, width_ / 2, height_ / 2 ),
    V_ ( GL_TEXTURE2, width_ / 2, height_ / 2 ),
    shader_()
{
  glLoadIdentity();
  glViewport( 0, 0, display_width_, display_height_ );
  glMatrixMode( GL_PROJECTION );
  glLoadIdentity();
  glOrtho( 0, display_width_, display_height_, 0, -1, 1 );
  glMatrixMode( GL_MODELVIEW );
  glLoadIdentity();
  glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
  glWindowPos2d( 0, 0 );
  glClear( GL_COLOR_BUFFER_BIT );
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
  glPushMatrix();
  glLoadIdentity();
  glTranslatef( 0, 0, 0 );
  glBegin( GL_POLYGON );

  const double xoffset = 0.25; /* MPEG-2 style 4:2:0 subsampling */

  glMultiTexCoord2d( GL_TEXTURE0, 0, 0 );
  glMultiTexCoord2d( GL_TEXTURE1, xoffset, 0 );
  glMultiTexCoord2d( GL_TEXTURE2, xoffset, 0 );
  glVertex2s( 0, 0 );

  glMultiTexCoord2d( GL_TEXTURE0, display_width_, 0 );
  glMultiTexCoord2d( GL_TEXTURE1, display_width_ / 2 + xoffset, 0 );
  glMultiTexCoord2d( GL_TEXTURE2, display_width_ / 2 + xoffset, 0 );
  glVertex2s( display_width_, 0 );

  glMultiTexCoord2d( GL_TEXTURE0, display_width_, display_height_ );
  glMultiTexCoord2d( GL_TEXTURE1, display_width_ / 2 + xoffset, display_height_ / 2 );
  glMultiTexCoord2d( GL_TEXTURE2, display_width_ / 2 + xoffset, display_height_ / 2 );
  glVertex2s( display_width_, display_height_ );

  glMultiTexCoord2d( GL_TEXTURE0, 0, display_height_ );
  glMultiTexCoord2d( GL_TEXTURE1, xoffset, display_height_ / 2 );
  glMultiTexCoord2d( GL_TEXTURE2, xoffset, display_height_ / 2 );
  glVertex2s( 0, display_height_ );

  glEnd();
  glPopMatrix();

  current_context_window_.window_.swap_buffers();
  // glfwPollEvents();
}
