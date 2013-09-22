#include <iostream>

#include "file.hh"

using namespace std;

const int IVF_frame_header_size = 12;

int main( int argc, char *argv[] )
{
  try {
    if ( argc != 2 ) {
      cerr << "Usage: " << argv[ 0 ] << " FILENAME" << endl;
      return EXIT_FAILURE;
    }

    const string filename( argv[ 1 ] );

    File file( filename );

    Block first_header = file.block( 0, 32 );

    if ( first_header.string( 0, 4 ) != "DKIF" ) {
      throw Exception( filename, "not an IVF file" );
    }

    if ( first_header.le16( 4 ) != 0 ) {
      throw Exception( filename, "unsupported IVF version" );
    }

    if ( first_header.le16( 6 ) != 32 ) {
      throw Exception( filename, "IVF header needs to be 32 bytes" );
    }

    string fourcc = first_header.string( 8, 4 );

    uint16_t width = first_header.le16( 12 ), height = first_header.le16( 14 );
    uint32_t frame_rate = first_header.le32( 16 ), time_scale = first_header.le32( 20 ),
      frame_count = first_header.le32( 24 );

    cout << "fourcc: " << fourcc << endl;
    cout << "width: " << width << endl;
    cout << "height: " << height << endl;
    cout << "frame_rate: " << frame_rate << endl;
    cout << "time_scale: " << time_scale << endl;
    cout << "frame_count: " << frame_count << endl;

    for ( uint64_t index = 32; index + IVF_frame_header_size <= file.size(); ) {
      uint32_t frame_size = le32toh( *reinterpret_cast<const uint32_t *>( &file[ index ] ) );
      uint64_t pts        = le64toh( *reinterpret_cast<const uint64_t *>( &file[ index + 4 ] ) );
      cout << "frame size: " << frame_size << ", pts: " << pts << endl;

      index += IVF_frame_header_size + frame_size;
    }
  } catch ( const Exception & e ) {
    e.perror();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
