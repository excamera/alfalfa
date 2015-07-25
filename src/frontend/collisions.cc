#include "continuation_player.hh"

#include <iostream>
#include <unordered_set> 
#include <unordered_map>
#include <vector>
#include <string>

using namespace std;

int main(int argc, char * argv[] ) {
  if ( argc != 2 ) {
    cerr << "Usage: " << argv[ 0 ] << " VIDEO\n"; 
  }

  // First decode the file and find potential collisions
  unordered_map<size_t, vector<RasterHandle>> raster_hashes;
  unordered_set<size_t> potential_collisions;
  ContinuationPlayer player( argv[ 1 ] );
  while ( not player.eof() ) {
    RasterHandle raster = player.next_output();
    size_t hash = raster.hash();

    // If a previous raster already had this hash then record it
    // as a potential collision
    if ( raster_hashes.count( hash ) ) {
      potential_collisions.insert( hash );
    }
    else {
      raster_hashes[ hash ] = vector<RasterHandle>();
    }
  }

  ContinuationPlayer check_player( argv[ 1 ] );
  while ( not check_player.eof() ) {
    RasterHandle raster = check_player.next_output();
    size_t hash = raster.hash();

    if ( potential_collisions.count( hash ) ) {
      for ( const RasterHandle & collision_raster : raster_hashes[ hash ] ) {
        if ( collision_raster.get() != raster.get() ) {
          cout << "collision on " << hash << "\n";
        }
      }
      raster_hashes[ hash ].push_back( raster );
    }
  }
}
