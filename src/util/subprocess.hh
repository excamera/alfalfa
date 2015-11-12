#ifndef SUBPROCESS_HH
#define SUBPROCESS_HH

#include <stdio.h>
#include <string>
#include <memory>

using namespace std;

class Subprocess
{
private:
  std::unique_ptr<FILE> file_{ nullptr };

public:
  Subprocess();
  void call( const string & command, const string & type = "w" );

  size_t write_string( const string & str );
  FILE * stream() { return file_.get(); }

  int wait();
  ~Subprocess();
};

#endif /* SUBPROCESS_HH */
