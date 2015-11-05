#include <fstream>

#include "frame.hh"
#include "uncompressed_chunk.hh"

using namespace std;

int main( int argc, char * argv[] )
{
  if ( argc < 3 ) {
    cerr << "Usage: " << argv[ 0 ] << "alf\n";
    return EXIT_FAILURE;
  }

  if ( chdir( argv[ 1 ] ) ) {
    throw unix_error( "chdir failed" );
  }

  ifstream original_manifest( "original_manifest" );
  uint16_t width, height;
  original_manifest >> width >> height;

  SerializedFrame compressed_frame( argv[ 2 ] );

  UncompressedChunk uncompressed_chunk( compressed_frame.chunk(), width, height );

  if ( uncompressed_chunk.key_frame() ) {
    cout << "Key Frame...\n";
  } else {
    cout << "Inter Frame: \n";
    /* initialize Boolean decoder for the frame and macroblock headers */
    BoolDecoder first_partition( uncompressed_chunk.first_partition() );

    /* parse interframe header */
    InterFrame myframe( uncompressed_chunk.show_frame(),
		        width, height, first_partition );

    ProbabilityTables fake_probs = ProbabilityTables();
    myframe.parse_macroblock_headers( first_partition, fake_probs );

    cout << myframe.stats() << endl;
  }
}
