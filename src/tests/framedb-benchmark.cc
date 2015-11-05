#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <iomanip>

#include "frame_db.hh"
#include "dependency_tracking.hh"

using namespace std;

int main()//( int argc, char const *argv[] )
{
  // loading frame_manifest
  vector<double> fdb_linear_search_times;
  vector<double> fdb_hashed_search_times;

  FrameDB fdb( "fake.db", "ALFAFRDB" );

  string line;
  string frame_name;
  size_t offset;
  size_t q;

  ifstream fin( "manifests/frame_manifest" );

  while ( getline( fin, line ) ) {
    istringstream ss( line );
    ss >> frame_name >> offset;

    SourceHash source( frame_name );
    TargetHash target( frame_name );
    FrameInfo fd( FrameName( source, target ), offset, 100 );
    fdb.insert(fd);
  }

  fin.close();

  cout << "frame_manifest read completed." << endl;

  vector<vector<FrameInfo> > streams;

  fin.open( "manifests/stream_manifest" );

  while ( getline( fin, line ) ) {
    istringstream ss( line );
    ss >> q >> frame_name;
    SourceHash source( frame_name );
    TargetHash target( frame_name );
    FrameInfo fd( FrameName( source, target ), 0, 0 );

    if( q >= streams.size() ) {
      streams.resize( q + 1 );
    }

    streams[ q ].emplace_back( fd );
  }

  fin.close();

  cout << "stream_manifest read completed." << endl;

  DecoderHash decoder_hash(0, 0, 0, 0);
  chrono::time_point<chrono::system_clock> start;
  chrono::duration<double> elapsed_seconds;

  int stream_idx = 0;

  for ( auto const & stream : streams ) {
    cout << "Stream " << stream_idx++ << "..." << endl;

    for ( auto const & frame : stream ) {
      decoder_hash.update( frame.target_hash() );

      // looking for the decoder hash using search_by_source_hash method.
      start = chrono::system_clock::now();
      auto result = fdb.search_by_decoder_hash( decoder_hash );

      for ( auto it = result.first; it != result.second; it++ ) {

      }
      elapsed_seconds = chrono::system_clock::now() - start;
      fdb_hashed_search_times.push_back( elapsed_seconds.count() );

      // looking for the decoder hash using linear search.
      start = chrono::system_clock::now();

      for ( auto it = fdb.begin(); it != fdb.end(); it++ ) {
        if ( decoder_hash.can_decode( it->source_hash() ) ) {

        }
      }

      elapsed_seconds = chrono::system_clock::now() - start;
      fdb_linear_search_times.push_back( elapsed_seconds.count() );
    }
  }

  sort( fdb_linear_search_times.begin(), fdb_linear_search_times.end() );
  sort( fdb_hashed_search_times.begin(), fdb_hashed_search_times.end() );

  double fdb_linear_search_mean = accumulate(fdb_linear_search_times.begin(), fdb_linear_search_times.end(), 0.0) /
   fdb_linear_search_times.size();
  double fdb_hashed_search_mean = accumulate(fdb_hashed_search_times.begin(), fdb_hashed_search_times.end(), 0.0) /
    fdb_hashed_search_times.size();

  double fdb_linear_search_median = fdb_linear_search_times[ fdb_linear_search_times.size() / 2 ];
  double fdb_hashed_search_median = fdb_hashed_search_times[ fdb_hashed_search_times.size() / 2 ];

  cout << "Linear mean:   " << fdb_linear_search_mean << endl;
  cout << "Linear median: " << fdb_linear_search_median << endl;

  cout << "Hashed mean:   " << fdb_hashed_search_mean << endl;
  cout << "Hashed median: " << fdb_hashed_search_median << endl;

  return 0;
}
