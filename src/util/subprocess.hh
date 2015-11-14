#ifndef SUBPROCESS_HH
#define SUBPROCESS_HH

#include <stdio.h>
#include <string>
#include <memory>

class Subprocess
{
private:
  std::unique_ptr<FILE> file_{ nullptr };

public:
  Subprocess();
  void call( const std::string & command, const std::string & type = "w" );

  size_t write_string( const std::string & str );
  FILE * stream() { return file_.get(); }

  int wait();
  ~Subprocess();
};

#endif /* SUBPROCESS_HH */
