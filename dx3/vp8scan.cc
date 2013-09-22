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

    if ( file.size() < 32 ) {
      throw Exception( filename, "not an IVF file" );
    }

    if ( file[ 0 ] != 'D' or file[ 1 ] != 'K' or
	 file[ 2 ] != 'I' or file[ 3 ] != 'F' ) {
      throw Exception( filename, "not an IVF file" );
    }

    uint16_t version = le16toh( *reinterpret_cast<const uint16_t *>( &file[ 4 ] ) );
    uint16_t hdr_len = le16toh( *reinterpret_cast<const uint16_t *>( &file[ 6 ] ) );
    string fourcc( reinterpret_cast<const char*>( &file[ 8 ] ), 4 );
    uint16_t width   = le16toh( *reinterpret_cast<const uint16_t *>( &file[ 12 ] ) );
    uint16_t height  = le16toh( *reinterpret_cast<const uint16_t *>( &file[ 14 ] ) );
    uint32_t frame_rate = le32toh( *reinterpret_cast<const uint32_t *>( &file[ 16 ] ) );
    uint32_t time_scale = le32toh( *reinterpret_cast<const uint32_t *>( &file[ 20 ] ) );
    uint32_t frame_count = le32toh( *reinterpret_cast<const uint32_t *>( &file[ 24 ] ) );

    cout << "version: " << version << endl;
    cout << "hdr_len: " << hdr_len << endl;
    cout << "fourcc: " << fourcc << endl;
    cout << "width: " << width << endl;
    cout << "height: " << height << endl;
    cout << "frame_rate: " << frame_rate << endl;
    cout << "time_scale: " << time_scale << endl;
    cout << "frame_count: " << frame_count << endl;

    for ( uint64_t index = hdr_len; index + IVF_frame_header_size <= file.size(); ) {
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
