/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Copyright 2013-2018 the Alfalfa authors
                       and the Massachusetts Institute of Technology

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

      1. Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include <cstdlib>

#include "ivf_writer.hh"

using namespace std;

int main( int argc, char *argv[] )
{
  if ( argc != 3 ) {
    cerr << "Usage: " << argv[ 0 ] << " ORIGINAL_FILENAME COPY_FILENAME" << endl;
    return EXIT_FAILURE;
  }

  IVF orig_file( argv[ 1 ] );
  IVF copy_file( argv[ 2 ] );

  //Compare header information between original and copy
  if(orig_file.fourcc() != copy_file.fourcc()) return EXIT_FAILURE;
  if(orig_file.width() != copy_file.width()) return EXIT_FAILURE;
  if(orig_file.height() != copy_file.height()) return EXIT_FAILURE;
  if(orig_file.frame_rate() != copy_file.frame_rate()) return EXIT_FAILURE;
  if(orig_file.time_scale() != copy_file.time_scale()) return EXIT_FAILURE;
  if(orig_file.frame_count() != copy_file.frame_count()) return EXIT_FAILURE;

  for ( unsigned int i = 0; i < orig_file.frame_count(); i++ ) {
    if(orig_file.frame(i).to_string() != copy_file.frame(i).to_string()) return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
