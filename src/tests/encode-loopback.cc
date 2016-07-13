/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <random>

#include "exception.hh"
#include "bool_decoder.hh"
#include "bool_encoder.hh"
#include "modemv_data.hh"

#include "tree.cc"
#include "encode_tree.cc"

using namespace std;

vector< uint8_t > encode( const vector< pair< Probability, bool > > & bitlist )
{
  /* encode the bits */
  BoolEncoder encoder;

  for ( const auto & x : bitlist ) {
    encoder.put( x.second, x.first );
  }

  return encoder.finish();
}

int main( int argc, char *argv[] )
{
  try {
    if ( argc != 1 ) {
      cerr << "Usage: " << argv[ 0 ] << endl;
      return EXIT_FAILURE;
    }

    /* Make some RNGs */
    random_device rd;
    default_random_engine gen( rd() );
    uniform_int_distribution< Probability > probs( 0, 255 );
    bernoulli_distribution bits;

    for ( unsigned int trial = 0; trial < 200; trial++ ) {
      /* Decide what we're going to encode */
      vector< pair< Probability, bool > > bitlist;
      for ( unsigned int i = 0; i < 10000; i++ ) {
        bitlist.emplace_back( probs( gen ), bits( gen ) );
      }

      const auto encoded_string = encode( bitlist );

      BoolDecoder decoder( Chunk( &encoded_string.front(), encoded_string.size() ) );

      for ( const auto & x : bitlist ) {
        const bool this_bit = decoder.get( x.first );
        if ( this_bit != x.second ) {
          cerr << "get( " << x.first << " ) got " << this_bit << ", expected " << x.second << endl;
          return EXIT_FAILURE;
        }
      }
    }

    /* Now try some trees */
    for ( unsigned int trial = 0; trial < 200; trial++ ) {
      for ( mbmode i = mbmode( 0 ); i < num_y_modes; i = mbmode( i + 1 ) ) {
        Tree< mbmode, num_y_modes, kf_y_mode_tree > test_mode = i;

        BoolEncoder encoder;

        ProbabilityArray< num_y_modes > probabilities;
        for ( uint8_t j = 0; j < probabilities.size(); j++ ) {
          probabilities.at( j ) = probs( gen );
        }

        encode( encoder, test_mode, probabilities );

        const auto encoded_string = encoder.finish();

        BoolDecoder decoder( Chunk( &encoded_string.front(), encoded_string.size() ) );

        const decltype( test_mode ) decoded_id = { decoder, probabilities };

        if ( decoded_id != i ) {
          cerr << "Encoded id of " << int(i) << " and got " << int(decoded_id) << endl;
          return EXIT_FAILURE;
        }
      }
    }
    
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
