/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef GL_OBJECTS_HH
#define GL_OBJECTS_HH

#define GLEW_STATIC

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <string>
#include <vector>
#include <memory>

#include "raster.hh"

class GLFWContext
{
  static void error_callback( const int, const char * const description );

public:
  GLFWContext();
  ~GLFWContext();

  /* forbid copy */
  GLFWContext( const GLFWContext & other ) = delete;
  GLFWContext & operator=( const GLFWContext & other ) = delete;
};

class Window
{
  struct Deleter { void operator() ( GLFWwindow * x ) const; };
  std::unique_ptr<GLFWwindow, Deleter> window_;

public:
  Window( const unsigned int width, const unsigned int height, const std::string & title,
          const bool fullscreen = false );
  void make_context_current( const bool initialize_extensions = false );
  bool should_close( void ) const;
  void swap_buffers( void );
  void hide_cursor( const bool hidden );
  bool key_pressed( const int key ) const;
  std::pair<unsigned int, unsigned int> size( void ) const;
  std::pair<unsigned int, unsigned int> window_size() const;
};

struct VertexObject
{
  float x[4];
  //float chroma_texture_x, chroma_texture_y;
};

template <GLenum id_>
class Buffer
{
public:
  Buffer() = delete;

  template <class T>
  static void bind( const T & obj )
  {
    glBindBuffer( id_, obj.num_ );
  }

  static void load( const std::vector<VertexObject> & vertices, const GLenum usage )
  {
    glBufferData( id, vertices.size() * sizeof( VertexObject ), &vertices.front(), usage );
  }

  constexpr static GLenum id = id_;
};

using ArrayBuffer = Buffer<GL_ARRAY_BUFFER>;

class VertexBufferObject
{
  friend ArrayBuffer;

  GLuint num_;

public:
  VertexBufferObject();
  ~VertexBufferObject();

  /* forbid copy */
  VertexBufferObject( const VertexBufferObject & other ) = delete;
  VertexBufferObject & operator=( const VertexBufferObject & other ) = delete;
};

class VertexArrayObject
{
  GLuint num_;

public:
  VertexArrayObject();
  ~VertexArrayObject();

  void bind( void );

  /* forbid copy */
  VertexArrayObject( const VertexArrayObject & other ) = delete;
  VertexArrayObject & operator=( const VertexArrayObject & other ) = delete;
};

class Texture
{
private:
  GLuint num_;
  unsigned int width_, height_;

public:
  Texture( const unsigned int width, const unsigned int height );
  ~Texture();

  void bind( const GLenum texture_unit );
  void load( const TwoD< uint8_t> & raster );
  void resize( const unsigned int width, const unsigned int height );
  std::pair<unsigned int, unsigned int> size( void ) const { return std::make_pair( width_, height_ ); }

  /* disallow copy */
  Texture( const Texture & other ) = delete;
  Texture & operator=( const Texture & other ) = delete;
};

void compile_shader( const GLuint num, const std::string & source );

template <GLenum type_>
class Shader
{
  friend class Program;

protected:
  GLuint num_ = glCreateShader( type_ );

public:
  Shader( const std::string & source )
  {
    compile_shader( num_, source );
  }

  ~Shader()
  {
    glDeleteShader( num_ );
  }

  /* forbid copy */
  Shader( const Shader & other ) = delete;
  Shader & operator=( const Shader & other ) = delete;
};

class Program
{
  GLuint num_ = glCreateProgram();

public:
  Program() {}
  ~Program();

  template <GLenum type_>
  void attach( const Shader<type_> & shader )
  {
    glAttachShader( num_, shader.num_ );
  }

  void link( void );
  void use( void );

  GLint attribute_location( const std::string & name ) const;
  GLint uniform_location( const std::string & name ) const;

  /* forbid copy */
  Program( const Program & other ) = delete;
  Program & operator=( const Program & other ) = delete;
};

using VertexShader   = Shader<GL_VERTEX_SHADER>;
using FragmentShader = Shader<GL_FRAGMENT_SHADER>;

void glCheck( const std::string & where, const bool expected = false );

#endif /* GL_OBJECTS_HH */
