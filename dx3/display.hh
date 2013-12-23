#ifndef DISPLAY_HH
#define DISPLAY_HH

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <GL/glx.h>

#include <functional>
#include <string>

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
  CheckedPointer< xcb_screen_t* > xcb_screen_;
  xcb_window_t window_;

public:
  XWindow( xcb_connection_t * connection,
	   const unsigned int display_width,
	   const unsigned int display_height );

  operator xcb_window_t() const { return window_; }
};

class GLContext
{
private:
  CheckedPointer< XVisualInfo* > visual_;
  CheckedPointer< GLXContext > context_;

public:
  GLContext( Display * display, const XWindow & window );
};

class GLTexture
{
private:
  GLuint id_;

public:
  GLTexture( const GLenum texture_unit,
	     const unsigned int width,
	     const unsigned int height );
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

public:
  VideoDisplay( const unsigned int display_width, const unsigned int display_height,
		const unsigned int raster_width, const unsigned int raster_height );

  VideoDisplay( const VideoDisplay & other ) = delete;
  VideoDisplay & operator=( const VideoDisplay & other ) = delete;
};

#endif
