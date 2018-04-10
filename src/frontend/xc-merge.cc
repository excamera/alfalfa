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

#include <getopt.h>
#include <iostream>

#include "ivf.hh"
#include "ivf_writer.hh"

using namespace std;

void usage_error( const string & program_name )
{
  cerr << "Usage: " << program_name << " <ivf1> <ivf2> [<output>]" << endl;
}


int main( int argc, char *argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  if ( argc < 3 ) {
    usage_error( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  string output_file = "output.ivf";

  if ( argc == 4 ) {
    output_file = argv[ 3 ];
  }

  IVF ivf1( argv[ 1 ] );
  IVF ivf2( argv[ 2 ] );

  if ( ivf1.width() != ivf2.width() or ivf1.height() != ivf2.height() ) {
    throw runtime_error( "cannot merge ivfs with different dimensions." );
  }

  IVFWriter output_ivf( output_file, "VP80", ivf1.width(), ivf1.height(), 1, 1 );

  for ( size_t i = 0; i < ivf1.frame_count(); i++ ) {
    output_ivf.append_frame( ivf1.frame( i ) );
  }

  for ( size_t i = 0; i < ivf2.frame_count(); i++ ) {
    output_ivf.append_frame( ivf2.frame( i ) );
  }

  return EXIT_SUCCESS;
}
