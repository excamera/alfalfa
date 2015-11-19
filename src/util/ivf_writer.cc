#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "ivf_writer.hh"
#include "safe_array.hh"
#include "mmap_region.hh"

using namespace std;

template <typename T> void zero( T & x ) { memset( &x, 0, sizeof( x ) ); }

static void memcpy_le16( uint8_t * dest, const uint16_t val )
{
  uint16_t swizzled = htole16( val );
  memcpy( dest, &swizzled, sizeof( swizzled ) );
}

static void memcpy_le32( uint8_t * dest, const uint32_t val )
{
  uint32_t swizzled = htole32( val );
  memcpy( dest, &swizzled, sizeof( swizzled ) );
}

IVFWriter::IVFWriter( const string & filename,
		      const string & fourcc,
		      const uint16_t width,
		      const uint16_t height,
		      const uint32_t frame_rate,
		      const uint32_t time_scale )
  : fd_( SystemCall( filename, open( filename.c_str(), O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH ) ) ),
    file_size_( 0 ),
    frame_count_( 0 )
{
  if ( fourcc.size() != 4 ) {
    throw internal_error( "IVF", "FourCC must be four bytes long" );
  }

  /* build the header */
  SafeArray<uint8_t, IVF::supported_header_len> new_header;
  zero( new_header );

  memcpy( &new_header.at( 0 ), "DKIF", 4 );
  /* skip version number (= 0) */
  memcpy_le16( &new_header.at( 6 ), IVF::supported_header_len );
  memcpy( &new_header.at( 8 ), fourcc.data(), 4 );
  memcpy_le16( &new_header.at( 12 ), width );
  memcpy_le16( &new_header.at( 14 ), height );
  memcpy_le32( &new_header.at( 16 ), frame_rate );
  memcpy_le32( &new_header.at( 20 ), time_scale );

  /* write the header */
  fd_.write( Chunk( &new_header.at( 0 ), new_header.size() ) );
  file_size_ += new_header.size();

  /* verify the new file size */
  assert( fd_.size() == file_size_ );
}

size_t IVFWriter::append_frame( const Chunk & chunk )
{
  /* map the header into memory */
  MMap_Region header_in_mem( IVF::supported_header_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd_.num() );
  uint8_t * mutable_header_ptr = header_in_mem.addr();

  Chunk header( mutable_header_ptr, IVF::supported_header_len );  

  /* verify the existing frame count */
  assert( frame_count_ == header( 24, 4 ).le32() );

  /* verify the existing file size */
  assert( file_size_ == fd_.size() );

  /* build the frame header */
  SafeArray<uint8_t, IVF::frame_header_len> new_header;
  zero( new_header );
  memcpy_le32( &new_header.at( 0 ), chunk.size() );

  /* XXX does not include presentation timestamp */

  /* append the frame header to the file */
  fd_.write( Chunk( &new_header.at( 0 ), new_header.size() ) );
  file_size_ += new_header .size();
  size_t written_offset = file_size_;

  /* append the frame to the file */
  fd_.write( chunk );
  file_size_ += chunk.size();

  /* verify the new file size */
  assert( fd_.size() == file_size_ );

  /* increment the frame count in the file's header */
  frame_count_++;
  memcpy_le32( mutable_header_ptr + 24, frame_count_ );

  /* verify the new frame count */
  assert( frame_count_ == header( 24, 4 ).le32() );

  return written_offset;
}
