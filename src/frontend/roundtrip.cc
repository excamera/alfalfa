#include <iostream>

#include "ivf.hh"
#include "encoder.hh"
#include "uncompressed_chunk.hh"

using namespace std;

int main( int argc, char *argv[] )
{
  try {
    if ( argc != 2 ) {
      cerr << "Usage: " << argv[ 0 ] << " FILENAME" << endl;
      return EXIT_FAILURE;
    }

    IVF file( argv[ 1 ] );

    if ( file.fourcc() != "VP80" ) {
      throw Unsupported( "not a VP8 file" );
    }

    /* find first key frame */
    uint32_t frame_no = 0;

    while ( frame_no < file.frame_count() ) {
      UncompressedChunk uncompressed_chunk( file.frame( frame_no ), file.width(), file.height() );
      if ( uncompressed_chunk.key_frame() ) {
	break;
      }
      frame_no++;
    }

    Encoder encoder( file.width(), file.height(), file.frame( frame_no ) );

    /* write IVF header */
    cout << "DKIF";
    cout << uint8_t(0) << uint8_t(0); /* version */
    cout << uint8_t(32) << uint8_t(0); /* header length */
    cout << "VP80"; /* fourcc */
    cout << uint8_t(file.width() & 0xff) << uint8_t((file.width() >> 8) & 0xff); /* width */
    cout << uint8_t(file.height() & 0xff) << uint8_t((file.height() >> 8) & 0xff); /* height */
    cout << uint8_t(1) << uint8_t(0) << uint8_t(0) << uint8_t(0); /* bogus frame rate */
    cout << uint8_t(1) << uint8_t(0) << uint8_t(0) << uint8_t(0); /* bogus time scale */

    const uint32_t le_num_frames = htole32( file.frame_count() - frame_no );
    cout << string( reinterpret_cast<const char *>( &le_num_frames ), sizeof( le_num_frames ) ); /* num frames */

    cout << uint8_t(0) << uint8_t(0) << uint8_t(0) << uint8_t(0); /* fill out header */

    for ( uint32_t i = frame_no; i < file.frame_count(); i++ ) {
      const vector< uint8_t > frame = encoder.encode_frame( file.frame( i ) );

      /* write size of frame */
      const uint32_t le_size = htole32( frame.size() );
      cout << string( reinterpret_cast<const char *>( &le_size ), sizeof( le_size ) ); /* size */

      cout << uint8_t(0) << uint8_t(0) << uint8_t(0) << uint8_t(0); /* fill out header */
      cout << uint8_t(0) << uint8_t(0) << uint8_t(0) << uint8_t(0); /* fill out header */

      fwrite( &frame.at( 0 ), frame.size(), 1, stdout );
    }
  } catch ( const Exception & e ) {
    e.perror( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
