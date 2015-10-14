#include <iostream>
#include <fstream>
#include <set>
#include <string>
#include <cstdlib>

#include "frame_db.hh"

using namespace std;

bool is_equal( const SourceHash & lhs, const DecoderHash & rhs )
{
  return ( not lhs.state_hash.initialized() or ( lhs.state_hash.get() == rhs.state_hash() ) ) and
    ( not lhs.continuation_hash.initialized() or ( lhs.continuation_hash.get() == rhs.continuation_hash() ) ) and
    ( not lhs.last_hash.initialized() or ( lhs.last_hash.get() == rhs.last_hash() ) ) and
    ( not lhs.golden_hash.initialized() or ( lhs.golden_hash.get() == rhs.golden_hash() ) ) and
    ( not lhs.alt_hash.initialized() or ( lhs.alt_hash.get() == rhs.alt_hash() ) );
}

int main( int argc, char const *argv[] )
{
  if ( argc < 3 ) {
    cerr << "Usage: framedb-test [input frame manifest] [output frame manifest]" << endl;
    return 1;
  }

  srand( time( 0 ) );

  FrameDB fdb( argv[1] );
  FrameDB fdb2( argv[2] );

  size_t state_hash, continuation_hash, last_hash, golden_hash, alt_hash;

  set<std::string> hashed_search_results, linear_search_results;

  for ( auto it = fdb.begin(); it != fdb.end(); it++ ) {
    state_hash = it->source_hash.state_hash.initialized() ? it->source_hash.state_hash.get() : rand();
    continuation_hash = it->source_hash.continuation_hash.initialized() ? it->source_hash.continuation_hash.get() : rand();
    last_hash = it->source_hash.last_hash.initialized() ? it->source_hash.last_hash.get() : rand();
    golden_hash = it->source_hash.golden_hash.initialized() ? it->source_hash.golden_hash.get() : rand();
    alt_hash = it->source_hash.alt_hash.initialized() ? it->source_hash.alt_hash.get() : rand();

    DecoderHash query_hash( state_hash, continuation_hash, last_hash,
      golden_hash, alt_hash );

    auto result = fdb.search_by_decoder_hash( query_hash );

    // hashed search
    for ( auto res_it = result.first; res_it != result.second; res_it++ ) {
      hashed_search_results.insert( res_it->frame_name );
    }

    // linear search
    for ( auto res_it = fdb.begin(); res_it != fdb.end(); res_it++ ) {
      if ( is_equal( res_it->source_hash, query_hash ) ) {
        linear_search_results.insert( res_it->frame_name );
      }
    }

    if ( hashed_search_results != linear_search_results ) {
      cout << "Inconsistency in hashed search results." << endl;
      return 1;
    }
  }

  for ( auto it = fdb.begin(); it != fdb.end(); it++ ) {
    fdb2.insert( *it );
  }

  fdb2.save();

  return 0;
}
