#ifndef DISPLAY_HH
#define DISPLAY_HH

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <GL/glx.h>

#include <functional>
#include <string>

#include "raster.hh"

template <typename T>
class CheckedPointer
{
private:
  T checked_pointer_;
  std::function<void(T)> deleter_;

public:
  CheckedPointer( const std::string & context,
		  T const pointer,
		  std::function<void(T)> && deleter = [](T){} );
  ~CheckedPointer() { deleter_( checked_pointer_ ); }

  CheckedPointer( const CheckedPointer & other ) = delete;
  CheckedPointer & operator=( const CheckedPointer & other ) = delete;

  operator T() const { return checked_pointer_; }
  T operator->() const { return checked_pointer_; }
};

class XWindow
{
private:
  xcb_connection_t * connection_;
  CheckedPointer< xcb_screen_t* > xcb_screen_;
  xcb_window_t window_;

public:
  XWindow( xcb_connection_t * connection,
	   const unsigned int display_width,
	   const unsigned int display_height );
  ~XWindow();

  operator xcb_window_t() const { return window_; }
  void destroy( xcb_connection_t * connection );

  XWindow( const XWindow & other ) = delete;
  XWindow & operator=( const XWindow & other ) = delete;
};

class GLContext
{
private:
  Display * display_;
  CheckedPointer< XVisualInfo* > visual_;
  CheckedPointer< GLXContext > context_;

public:
  GLContext( Display * display, const XWindow & window );

  GLContext( const GLContext & other ) = delete;
  GLContext & operator=( const GLContext & other ) = delete;
};

class GLTexture
{
private:
  GLenum texture_unit_;
  GLuint id_;
  unsigned int width_, height_;

public:
  GLTexture( const GLenum texture_unit,
	     const unsigned int width,
	     const unsigned int height );
  ~GLTexture();
  void load( const TwoD< uint8_t > & raster );
};

class GLShader
{
private:
  GLuint id_;

public:
  GLShader();
  ~GLShader();
};

class VideoDisplay
{
private:
  unsigned int display_width_, display_height_;
  unsigned int raster_width_, raster_height_;

  CheckedPointer< Display* > display_;
  CheckedPointer< xcb_connection_t* > xcb_connection_;
  XWindow window_;
  GLContext context_;

  /* textures */
  GLTexture Y_, Cb_, Cr_;

  /* colorspace transformation shader */
  GLShader shader_;

public:
  VideoDisplay( const BaseRaster & raster );

  VideoDisplay( const VideoDisplay & other ) = delete;
  VideoDisplay & operator=( const VideoDisplay & other ) = delete;

  void draw( const BaseRaster & raster );
  void repaint( void );
};

#endif
