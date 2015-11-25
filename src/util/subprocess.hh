#ifndef SUBPROCESS_HH
#define SUBPROCESS_HH

#include <cstdio>
#include <string>
#include <memory>

#include "chunk.hh"

/* wraps popen */
class Subprocess
{
private:
  struct Deleter { void operator() ( FILE * x ) const; };
  std::unique_ptr<FILE, Deleter> file_;

public:
  Subprocess( const std::string & command, const std::string & type );

  void write( const Chunk & chunk );

  void close(); /* may throw exception, so better for caller to call explicitly
		   instead of letting destructor possibly throw */
};

#endif /* SUBPROCESS_HH */
