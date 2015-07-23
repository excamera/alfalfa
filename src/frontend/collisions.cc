#include "decoder.hh"
#include "player.hh"
#include "file_descriptor.hh"

#include <iostream>
#include <unordered_set> 
#include <fstream>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

using namespace std;

int main(int argc, char * argv[] ) {
  if ( argc != 3 ) {
    cerr << "Usage: " << argv[ 0 ] << " VIDEO STORAGE_DIR\n"; 
  }

  if ( mkdir( argv[ 2 ], 0777 ) ) {
    throw unix_error( "mkdir failed" );
  }

  if ( chdir( argv[ 2 ] ) ) {
    throw unix_error( "chdir failed" );
  }

  // We save rasters to disk otherwise we use too much ram
  unordered_set<size_t> raster_hashes;
  FilePlayer player( argv[ 1 ] );
  while ( not player.eof() ) {
    RasterHandle raster = player.advance();

    size_t hash = raster.hash();

    string hash_str = to_string( hash );

    if ( raster_hashes.count( hash ) ) {
      FileDescriptor old_fd( SystemCall( hash_str, open( hash_str.c_str(), O_RDONLY ) ) );
      void * old_dump = mmap( nullptr, old_fd.size(), PROT_READ, MAP_SHARED, old_fd.num(), 0 );

      auto y_begin = raster.get().Y().begin();
      unsigned y_size = raster.get().Y().end() - y_begin;
      if ( memcmp( &*y_begin, old_dump, y_size ) != 0 ) {
        cout << "Raster hash collision!\n";
      }
      old_dump = (char *)old_dump + y_size;
      auto u_begin = raster.get().U().begin();
      unsigned u_size = raster.get().U().end() - u_begin;
      if ( memcmp( &*u_begin, old_dump, u_size ) != 0 ) {
        cout << "Raster hash collision!\n";
      }
      old_dump = (char *)old_dump + u_size;
      auto v_begin = raster.get().V().begin();
      unsigned v_size = raster.get().V().end() - v_begin;
      if ( memcmp( &*v_begin, old_dump, v_size ) != 0 ) {
        cout << "Raster hash collision!\n";
      }
      munmap(old_dump, old_fd.size());
    }
    else {
      raster_hashes.insert( hash );
      FILE * new_raster = fopen( hash_str.c_str(), "w" ); 
      raster.get().dump( new_raster );
      fclose( new_raster );
    }
  }
}
