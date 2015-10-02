#ifndef DISPLAY_HH
#define DISPLAY_HH

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "raster.hh"
#include "gl_objects.hh"

using namespace std;

class VideoDisplay
{
private:
  unsigned int display_width_, display_height_;
  unsigned int width_, height_;

  struct CurrentContextWindow
  {
    GLFWContext glfw_context_ = {};
    Window window_;

    CurrentContextWindow( const unsigned int width, const unsigned int height,
      const std::string & title );
  } current_context_window_;

  Texture Y_, U_, V_;
  AhabShader shader_;

public:
  VideoDisplay( const BaseRaster & raster );
  VideoDisplay( const VideoDisplay & other ) = delete;
  VideoDisplay & operator=( const VideoDisplay & other ) = delete;

  void draw( const BaseRaster & raster );
  void repaint( void );

  const Window & window( void ) const { return current_context_window_.window_; }
};

#endif /* DISPLAY_HH */
