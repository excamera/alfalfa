/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

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
