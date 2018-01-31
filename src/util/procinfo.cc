/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include "procinfo.hh"

#include <fstream>

using namespace std;

size_t procinfo::memory_usage()
{
  ifstream fin( "/proc/self/statm" );
  size_t v;
  fin >> v;
  return v * 4096 /* page size */;
}
