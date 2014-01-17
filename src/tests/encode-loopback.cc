#include <random>

#include "exception.hh"
#include "bool_decoder.hh"
#include "bool_encoder.hh"

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
  } catch ( const Exception & e ) {
    e.perror( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
