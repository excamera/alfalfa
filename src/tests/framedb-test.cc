#include <iostream>
#include <fstream>
#include <set>
#include <string>
#include <cstdlib>

#include "frame_db.hh"

using namespace std;

/* Creates a simple mock frame db, and cheks to make sure invariants provided
   by the different indices are maintained. */
int main()
{
  string db1 = "test-frame-db";

  FrameDB fdb1( db1, "ALFAFRDB", OpenMode::TRUNCATE );
  set<size_t> frame_ids;

  for( size_t i = 0; i < 3048; i++ ) {
    SourceHash source_hash(
      make_optional( ( i >> 0 ) & 1, i * 1234 + 0 ),
      make_optional( ( i >> 2 ) & 1, i * 1234 + 2 ),
      make_optional( ( i >> 3 ) & 1, i * 1234 + 3 ),
      make_optional( ( i >> 4 ) & 1, i * 1234 + 4 )
    );

    UpdateTracker update_tracker( ( i % 2 ) & 1, ( i % 3 ) & 1, ( i % 4 ) & 1 , ( i % 5 ) & 1,
      ( i % 6 ) & 1, ( i % 7 ) & 1, ( i % 8 ) & 1 );

    TargetHash target_hash( update_tracker, i * 1234 + 5,
      i * 1234 + 6, ( i % 5 ) &  1 );

    FrameInfo fi( FrameName( source_hash, target_hash ), i * 4312, i * 2134 );

    size_t frame_id = fdb1.insert( fi );
    /* Shouldn't have any duplicates in frame_ids generated. */
    if ( frame_ids.count( frame_id ) > 0 )
      return 1;
    frame_ids.insert( frame_id );
  }

  fdb1.serialize();

  FrameDB fdb2( db1, "ALFAFRDB", OpenMode::READ );

  size_t state_hash, last_hash, golden_hash, alt_hash;
  set<string> hashed_search_results, linear_search_results;

  for ( auto it = fdb1.begin(); it != fdb1.end(); it++ ) {
    state_hash = it->source_hash().state_hash.initialized() ? it->source_hash().state_hash.get() : rand();
    last_hash = it->source_hash().last_hash.initialized() ? it->source_hash().last_hash.get() : rand();
    golden_hash = it->source_hash().golden_hash.initialized() ? it->source_hash().golden_hash.get() : rand();
    alt_hash = it->source_hash().alt_hash.initialized() ? it->source_hash().alt_hash.get() : rand();

    DecoderHash query_hash( state_hash, last_hash, golden_hash, alt_hash );

    auto result = fdb2.search_by_decoder_hash( query_hash );

    for ( auto res_it = result.first; res_it != result.second; res_it++ ) {
      hashed_search_results.insert( res_it->frame_name() );
    }

    // linear search
    for ( auto res_it = fdb1.begin(); res_it != fdb1.end(); res_it++ ) {
      if ( query_hash.can_decode( res_it->source_hash() ) ) {
        linear_search_results.insert( res_it->frame_name() );
      }
    }

    if ( hashed_search_results != linear_search_results ) {
      cout << "Inconsistency in hashed search results." << endl;
      return 1;
    }
  }

  remove( db1.c_str() );

  return 0;
}
