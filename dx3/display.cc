#define GL_GLEXT_PROTOTYPES

#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <GL/glu.h>

#include "display.hh"
#include "exception.hh"
#include "ahab_shader.hh"

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
  : connection_( connection ),
    xcb_screen_( "xcb_setup_roots_iterator",
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

XWindow::~XWindow( void )
{
  xcb_destroy_window( connection_, window_ );
}

GLTexture::GLTexture( const GLenum texture_unit,
		      const unsigned int width,
		      const unsigned int height )
  : id_( -1 )
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

GLTexture::~GLTexture()
{
  glDeleteTextures( 1, &id_ );
}

static int desired_attributes[] = { GLX_RGBA,
				    GLX_DOUBLEBUFFER, True,
				    GLX_RED_SIZE, 8,
				    GLX_GREEN_SIZE, 8,
				    GLX_BLUE_SIZE, 8,
				    None };

GLContext::GLContext( Display * display, const XWindow & window )
  : display_( display ),
    visual_( "glXChooseVisual",
	     glXChooseVisual( display, 0, desired_attributes ),
	     []( XVisualInfo * x ) { XFree( x ); } ),
    context_( "glXCreateContext",
	      glXCreateContext( display, visual_, nullptr, true ),
	      [&]( GLXContext x ) { glXDestroyContext( display_, x ); } )
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
  Cr_( GL_TEXTURE0, raster_width_/2, raster_height_/2 ),
  shader_()
{
  xcb_flush( xcb_connection_ );
}

GLShader::GLShader()
  : id_( -1 )
{
  glEnable( GL_FRAGMENT_PROGRAM_ARB );
  glGenProgramsARB( 1, &id_ );
  glBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, id_ );
  glProgramStringARB( GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                      strlen( ahab_shader ),
                      ahab_shader );

  /* check for errors */
  GLint error_location;
  glGetIntegerv( GL_PROGRAM_ERROR_POSITION_ARB, &error_location );
  if ( error_location != -1 ) {
    throw Exception( "loading colorspace transformation shader",
		     reinterpret_cast<const char *>( glGetString( GL_PROGRAM_ERROR_STRING_ARB ) ) );
  }

  GLcheck( "glProgramString" );

  /* load Rec. 709 colors */
  glProgramLocalParameter4dARB( GL_FRAGMENT_PROGRAM_ARB, 0,
                                itu709_green[ 0 ], itu709_green[ 1 ], itu709_green[ 2 ], 0 );
  glProgramLocalParameter4dARB( GL_FRAGMENT_PROGRAM_ARB, 1,
                                itu709_blue[ 0 ], itu709_blue[ 1 ], itu709_blue[ 2 ], 0 );
  glProgramLocalParameter4dARB( GL_FRAGMENT_PROGRAM_ARB, 2,
                                itu709_red[ 0 ], itu709_red[ 1 ], itu709_red[ 2 ], 0 );

  GLcheck( "glProgramEnvParamater4d" );
}

GLShader::~GLShader()
{
  glDeleteProgramsARB( 1, &id_ );
}
