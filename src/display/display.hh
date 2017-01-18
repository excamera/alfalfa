/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef DISPLAY_HH
#define DISPLAY_HH

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "raster.hh"
#include "gl_objects.hh"

class VideoDisplay
{
private:
  static const std::string shader_source_scale_from_pixel_coordinates;
  static const std::string shader_source_ycbcr;

  unsigned int display_width_, display_height_;
  unsigned int width_, height_;

  struct CurrentContextWindow
  {
    GLFWContext glfw_context_ = {};
    Window window_;

    CurrentContextWindow( const unsigned int width, const unsigned int height,
      const std::string & title, const bool fullscreen );
  } current_context_window_;

  VertexShader scale_from_pixel_coordinates_ = { shader_source_scale_from_pixel_coordinates };
  FragmentShader ycbcr_shader_ = { shader_source_ycbcr };

  Program texture_shader_program_ = {};

  Texture Y_, U_, V_;

  VertexArrayObject texture_shader_array_object_ = {};
  VertexBufferObject screen_corners_ = {};
  VertexBufferObject other_vertices_ = {};

public:
  VideoDisplay( const BaseRaster & raster, const bool fullscreen = false );

  VideoDisplay( const VideoDisplay & other ) = delete;
  VideoDisplay & operator=( const VideoDisplay & other ) = delete;

  void draw( const BaseRaster & raster );
  void repaint( void );
  void resize( const std::pair<unsigned int, unsigned int> & target_size );

  const Window & window( void ) const { return current_context_window_.window_; }
};

#endif /* DISPLAY_HH */
