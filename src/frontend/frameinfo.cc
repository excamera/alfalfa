#include <fstream>

#include "frame.hh"
#include "uncompressed_chunk.hh"
#include "alfalfa_video.hh"

using namespace std;

int main( int argc, char * argv[] )
{
  if ( argc < 3 ) {
    cerr << "Usage: " << argv[ 0 ] << " alf frame_name\n";
    return EXIT_FAILURE;
  }

  PlayableAlfalfaVideo alf( argv[ 1 ] );

  const unsigned width = alf.get_info().width;
  const unsigned height = alf.get_info().height;

  FrameName name( argv[ 2 ] );

  if ( not alf.has_frame_name( name ) ) {
    cerr << argv[ 1 ] << " not in FrameDB\n";
    return EXIT_FAILURE;
  }

  FrameInfo frame = alf.get_frame( name );

  UncompressedChunk uncompressed_chunk( alf.get_chunk( frame ),
                                        width,
                                        height );

  if ( uncompressed_chunk.key_frame() ) {
    cout << "Key Frame...\n";
  } else if ( uncompressed_chunk.experimental() ) {
    if ( uncompressed_chunk.reference_update() ) {
      cout << "Reference Update Frame\n";
    } else {
      cout << "State Update Frame\n";
    }
  } else {
    cout << "Inter Frame: \n";
    /* initialize Boolean decoder for the frame and macroblock headers */
    BoolDecoder first_partition( uncompressed_chunk.first_partition() );

    InterFrame myframe( uncompressed_chunk.show_frame(),
		        width, height, first_partition );

    /* We don't need the probabilities to be accurate since
     * decoding the macroblock header parts we care about only
     * uses the frame header */
    ProbabilityTables fake_probs = ProbabilityTables();
    myframe.parse_macroblock_headers( first_partition, fake_probs );

    cout << myframe.stats() << endl;
  }
}
