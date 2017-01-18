/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <iostream>
#include <stdexcept>
#include <memory>

#include "gl_objects.hh"
#include "exception.hh"

using namespace std;

GLFWContext::GLFWContext()
{
  glfwSetErrorCallback( error_callback );

  glfwInit();
}

void GLFWContext::error_callback( const int, const char * const description )
{
  throw runtime_error( description );
}

GLFWContext::~GLFWContext()
{
  glfwTerminate();
}

Window::Window( const unsigned int width, const unsigned int height,
                const string & title, const bool fullscreen )
  : window_()
{
  glfwDefaultWindowHints();

  glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
  glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 1 );
  glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );

  glfwWindowHint( GLFW_RESIZABLE, GL_TRUE );

  window_.reset( glfwCreateWindow( width, height, title.c_str(),
                                   fullscreen ? glfwGetPrimaryMonitor() : nullptr, nullptr ) );
  if ( not window_.get() ) {
    throw runtime_error( "could not create window" );
  }
}

void Window::make_context_current( const bool initialize_extensions )
{
  glfwMakeContextCurrent( window_.get() );

  glCheck( "after MakeContextCurrent" );

  if ( initialize_extensions ) {
    glewExperimental = GL_TRUE;
    glewInit();
    glCheck( "after initializing GLEW", true );
  }
}

bool Window::should_close( void ) const
{
  return glfwWindowShouldClose( window_.get() );
}

void Window::swap_buffers( void )
{
  return glfwSwapBuffers( window_.get() );
}

void Window::hide_cursor( const bool hidden )
{
  glfwSetInputMode( window_.get(), GLFW_CURSOR, hidden ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL );
}

bool Window::key_pressed( const int key ) const
{
  return GLFW_PRESS == glfwGetKey( window_.get(), key );
}

pair<unsigned int, unsigned int> Window::size( void ) const
{
  int width, height;
  glfwGetFramebufferSize( window_.get(), &width, &height );

  if ( width < 0 or height < 0 ) {
    throw runtime_error( "negative framebuffer width or height" );
  }

  return pair<unsigned int, unsigned int>( width, height );
}

pair<unsigned int, unsigned int> Window::window_size() const
{
  int width, height;
  glfwGetWindowSize( window_.get(), &width, &height );

  if ( width < 0 or height < 0 ) {
    throw runtime_error( "negative window width or height" );
  }

  return pair<unsigned int, unsigned int>( width, height );
}

void Window::Deleter::operator() ( GLFWwindow * x ) const
{
  glfwHideWindow( x );
  glfwDestroyWindow( x );
}

VertexBufferObject::VertexBufferObject()
  : num_()
{
  glGenBuffers( 1, &num_ );
}

VertexBufferObject::~VertexBufferObject()
{
  glDeleteBuffers( 1, &num_ );
}

VertexArrayObject::VertexArrayObject()
  : num_()
{
  glGenVertexArrays( 1, &num_ );
}

VertexArrayObject::~VertexArrayObject()
{
  glDeleteVertexArrays( 1, &num_ );
}

void VertexArrayObject::bind( void )
{
  glBindVertexArray( num_ );
}

Texture::Texture( const unsigned int width, const unsigned int height )
  : num_(),
    width_( width ),
    height_( height )
{
  glGenTextures( 1, &num_ );
}

Texture::~Texture()
{
  glDeleteTextures( 1, &num_ );
}

void Texture::bind( const GLenum texture_unit )
{
  glActiveTexture( texture_unit );
  resize( width_, height_ );

  glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
  glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
  glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
  glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
}

void Texture::resize( const unsigned int width, const unsigned int height )
{
  width_ = width;
  height_ = height;
  glBindTexture( GL_TEXTURE_RECTANGLE, num_ );
  glTexImage2D( GL_TEXTURE_RECTANGLE, 0, GL_RGBA8, width_, height_, 0,
                GL_BGRA, GL_UNSIGNED_BYTE, nullptr );
}

void Texture::load( const TwoD< uint8_t > & raster )
{
  /*if ( image.size() != size() ) {
    throw runtime_error( "image size does not match texture dimensions" );
  }*/

  glBindTexture( GL_TEXTURE_RECTANGLE, num_ );
  glPixelStorei( GL_UNPACK_ROW_LENGTH, width_ );
  glTexSubImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, width_, height_,
                   GL_LUMINANCE, GL_UNSIGNED_BYTE, &( raster.at( 0, 0 ) ) );
}

void compile_shader( const GLuint num, const string & source )
{
  const char * source_c_str = source.c_str();
  glShaderSource( num, 1, &source_c_str, nullptr );
  glCompileShader( num );

  /* check if there were log messages */
  GLint log_length;
  glGetShaderiv( num, GL_INFO_LOG_LENGTH, &log_length );

  if ( log_length > 1 ) {
    unique_ptr<GLchar> buffer( new GLchar[ log_length ] );
    GLsizei written_length;
    glGetShaderInfoLog( num, log_length, &written_length, buffer.get() );

    if ( written_length + 1 != log_length ) {
      throw runtime_error( "GL shader log size mismatch" );
    }

    cerr << "GL shader compilation log: " << string( buffer.get(), log_length ) << endl;
  }

  /* check if it compiled */
  GLint success;
  glGetShaderiv( num, GL_COMPILE_STATUS, &success );

  if ( not success ) {
    throw runtime_error( "GL shader failed to compile" );
  }
}

void Program::link( void )
{
  glLinkProgram( num_ );
}

void Program::use( void )
{
  glUseProgram( num_ );
}

GLint Program::attribute_location( const string & name ) const
{
  const GLint ret = glGetAttribLocation( num_, name.c_str() );
  if ( ret < 0 ) {
    throw runtime_error( "attribute not found: " + name );
  }
  return ret;
}

GLint Program::uniform_location( const string & name ) const
{
  const GLint ret = glGetUniformLocation( num_, name.c_str() );
  if ( ret < 0 ) {
    throw runtime_error( "uniform not found: " + name );
  }
  return ret;
}

Program::~Program()
{
  glDeleteProgram( num_ );
}

void glCheck( const string & where, const bool expected )
{
  GLenum error = glGetError();

  if ( error != GL_NO_ERROR ) {
    while ( error != GL_NO_ERROR ) {
      if ( not expected ) {
        cerr << "GL error " << where << ": " << gluErrorString( error ) << endl;
      }
      error = glGetError();
    }

    if ( not expected ) {
      throw runtime_error( "GL error " + where );
    }
  }
}
