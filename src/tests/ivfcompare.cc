/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

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
