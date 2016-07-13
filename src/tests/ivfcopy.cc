/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <cstdlib>

#include "ivf_writer.hh"

using namespace std;

int main( int argc, char *argv[] )
{
  if ( argc != 3 ) {
    cerr << "Usage: " << argv[ 0 ] << " OLD_FILENAME NEW_FILENAME" << endl;
    return EXIT_FAILURE;
  }

  IVF old_file( argv[ 1 ] );
  IVFWriter new_file( argv[ 2 ],
                      old_file.fourcc(),
                      old_file.width(),
                      old_file.height(),
                      old_file.frame_rate(),
                      old_file.time_scale() );
  
  for ( unsigned int i = 0; i < old_file.frame_count(); i++ ) {
    new_file.append_frame( old_file.frame( i ) );
  }
}
