#include <GL/glu.h>

#include "display.hh"
#include "exception.hh"

using namespace std;

static void GLcheck( const string & description )
{
  const GLenum gl_error = glGetError();

  if ( gl_error != GL_NO_ERROR ) {
    throw Exception( description, reinterpret_cast<const char *>( gluErrorString( gl_error ) ) );
  }
}

template <class T>
CheckedPointer<T>::CheckedPointer( const string & context, T const pointer, std::function<void(T)> && deleter )
  : checked_pointer_( pointer ), deleter_( deleter )
{
  if ( checked_pointer_ == nullptr ) {
    throw Exception( context, "returned null pointer" );
  }
}

XWindow::XWindow( xcb_connection_t * connection,
		  const unsigned int display_width,
		  const unsigned int display_height )
  : xcb_screen_( "xcb_setup_roots_iterator",
		 xcb_setup_roots_iterator( xcb_get_setup( connection ) ).data ),
    window_( xcb_generate_id( connection ) )
{
  xcb_create_window( connection,
		     XCB_COPY_FROM_PARENT,
		     window_,
		     xcb_screen_->root,
		     0, 0,
		     display_width, display_height,
		     0,
		     XCB_WINDOW_CLASS_INPUT_OUTPUT,
		     xcb_screen_->root_visual,
		     0, nullptr );

  xcb_map_window( connection, window_ );

  xcb_flush( connection );
}

GLTexture::GLTexture( const GLenum texture_unit,
		      const unsigned int width,
		      const unsigned int height )
  : id_()
{
  glActiveTexture( texture_unit );
  glEnable( GL_TEXTURE_RECTANGLE_ARB );
  glGenTextures( 1, &id_ );
  glBindTexture( GL_TEXTURE_RECTANGLE_ARB, id_ );
  glTexImage2D( GL_TEXTURE_RECTANGLE_ARB, 0,
                GL_LUMINANCE8, width, height,
                0, GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr );
  glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
  glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
  glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
  glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
  glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
  GLcheck( "GLTexture" );
}

static int desired_attributes[] = { GLX_RGBA,
				    GLX_DOUBLEBUFFER, True,
				    GLX_RED_SIZE, 8,
				    GLX_GREEN_SIZE, 8,
				    GLX_BLUE_SIZE, 8,
				    None };

GLContext::GLContext( Display * display, const XWindow & window )
  : visual_( "glXChooseVisual",
	     glXChooseVisual( display, 0, desired_attributes ),
	     []( XVisualInfo * x ) { XFree( x ); } ),
    context_( "glXCreateContext",
	      glXCreateContext( display, visual_, nullptr, true ),
	      [&]( GLXContext x ) { glXDestroyContext( display, x ); } )
{
  if ( not glXMakeCurrent( display, window, context_ ) ) {
    throw Exception( "glXMakeCurrent", "failed" );
  }

  GLcheck( "glXMakeCurrent" );
}

VideoDisplay::VideoDisplay( const unsigned int display_width, const unsigned int display_height,
			    const unsigned int raster_width, const unsigned int raster_height )
  : display_width_( display_width ), display_height_( display_height ),
    raster_width_( raster_width ), raster_height_( raster_height ),
    display_( "XOpenDisplay", XOpenDisplay( nullptr ), []( Display *x ) { XCloseDisplay( x ); } ),
    xcb_connection_( "XGetXCBConnection",
		     ( XSetEventQueueOwner( display_, XCBOwnsEventQueue ),
		       XGetXCBConnection( display_ ) ) ),
  window_( xcb_connection_, display_width, display_height ),
  context_( display_, window_ ),
  Y_( GL_TEXTURE0, raster_width_, raster_height_ ),
  Cb_( GL_TEXTURE0, raster_width_/2, raster_height_/2 ),
  Cr_( GL_TEXTURE0, raster_width_/2, raster_height_/2 )
{
  
}

